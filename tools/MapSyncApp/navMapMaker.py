import os
import math
import shutil
import threading
import struct
import xml.etree.ElementTree as ET
import tkinter as tk
from tkinter import filedialog, messagebox
import tkintermapview
import requests
from PIL import Image, ImageTk
from io import BytesIO
import glob

# --- CORE SETTINGS ---
ZOOM_LEVEL = 15
USER_AGENT = "DiamantBikeComputer/2.0 (Contact: your@email.com)"
PADDING_DEGREES = 0.02 # Voegt een ~2km buffer toe rond je route

class BikeComputerTool(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Diamant Bike Computer Sync Tool")
        self.geometry("900x700")
        
        self.gpx_path = tk.StringVar()
        self.sd_path = tk.StringVar()
        self.output_dir = os.path.join(os.getcwd(), "output_tiles")
        
        self.setup_ui()

    def setup_ui(self):
        # --- LEFT PANEL (Controls) ---
        control_frame = tk.Frame(self, width=300)
        control_frame.pack(side=tk.LEFT, fill=tk.Y, padx=10, pady=10)

        # 1. GPX
        tk.Label(control_frame, text="1. Selecteer Route", font=("Arial", 10, "bold")).pack(pady=(10, 0))
        tk.Entry(control_frame, textvariable=self.gpx_path, width=40, state='readonly').pack()
        tk.Button(control_frame, text="Browse GPX", command=self.browse_gpx).pack(pady=5)

        # 2. SD Card (Optional)
        tk.Label(control_frame, text="2. Selecteer SD Kaart (Optioneel)", font=("Arial", 10, "bold")).pack(pady=(10, 0))
        tk.Entry(control_frame, textvariable=self.sd_path, width=40, state='readonly').pack()
        tk.Button(control_frame, text="Browse SD", command=self.browse_sd).pack(pady=5)

        # 3. Process
        self.btn_process = tk.Button(control_frame, text="3. Process & Sync", bg="green", fg="white", 
                                     font=("Arial", 12, "bold"), command=self.start_processing)
        self.btn_process.pack(pady=20)

        # 4. Viewer
        self.btn_viewer = tk.Button(control_frame, text="4. Bekijk Gegenereerde Tiles", bg="blue", fg="white",
                                    font=("Arial", 12, "bold"), command=self.open_tile_viewer)
        self.btn_viewer.pack(pady=5)

        tk.Label(control_frame, text="Log Output:", font=("Arial", 10, "bold")).pack(anchor="w", pady=(20,0))
        self.log_text = tk.Text(control_frame, height=13, width=40)
        self.log_text.pack()

        # --- RIGHT PANEL (Map View) ---
        map_frame = tk.Frame(self)
        map_frame.pack(side=tk.RIGHT, fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        self.map_widget = tkintermapview.TkinterMapView(map_frame, corner_radius=0)
        self.map_widget.pack(fill=tk.BOTH, expand=True)
        self.map_widget.set_position(50.8503, 4.3517) # Standaard op Brussel
        self.map_widget.set_zoom(8)

    def log(self, message):
        self.log_text.insert(tk.END, message + "\n")
        self.log_text.see(tk.END)
        self.update()

    def browse_gpx(self):
        file = filedialog.askopenfilename(filetypes=[("GPX Files", "*.gpx")])
        if file:
            self.gpx_path.set(file)
            self.draw_route_on_map(file)

    def browse_sd(self):
        folder = filedialog.askdirectory(title="Selecteer SD Kaart Map")
        if folder:
            self.sd_path.set(folder)

    def draw_route_on_map(self, gpx_file):
        try:
            tree = ET.parse(gpx_file)
            root = tree.getroot()
            
            coords = []
            for elem in root.iter():
                if 'lat' in elem.attrib and 'lon' in elem.attrib:
                    coords.append((float(elem.attrib['lat']), float(elem.attrib['lon'])))
            
            if coords:
                self.map_widget.delete_all_path()
                self.map_widget.set_path(coords, color="red", width=3)
                self.map_widget.fit_bounding_box(
                    (max(c[0] for c in coords), min(c[1] for c in coords)), 
                    (min(c[0] for c in coords), max(c[1] for c in coords))
                )
        except Exception as e:
            messagebox.showerror("Error", f"Failed to parse GPX for map: {e}")

    def start_processing(self):
        if not self.gpx_path.get():
            messagebox.showerror("Error", "Selecteer eerst een GPX bestand.")
            return
        
        self.btn_process.config(state=tk.DISABLED)
        self.log("Start met verwerken...")
        threading.Thread(target=self.pipeline, daemon=True).start()

    def pipeline(self):
        try:
            tree = ET.parse(self.gpx_path.get())
            root = tree.getroot()
            lats = [float(elem.attrib['lat']) for elem in root.iter() if 'lat' in elem.attrib]
            lons = [float(elem.attrib['lon']) for elem in root.iter() if 'lon' in elem.attrib]

            lat_min, lat_max = min(lats) - PADDING_DEGREES, max(lats) + PADDING_DEGREES
            lon_min, lon_max = min(lons) - PADDING_DEGREES, max(lons) + PADDING_DEGREES

            os.makedirs(self.output_dir, exist_ok=True)
            self.download_and_convert(lat_min, lat_max, lon_min, lon_max, self.output_dir)
            self.log(f"✅ Lokaal opgeslagen in {self.output_dir}")

            # SD Card Logica
            sd_folder = self.sd_path.get()
            if sd_folder:
                self.log("Kopiëren naar SD-kaart...")
                sd_tiles_dir = os.path.join(sd_folder, "tiles")
                shutil.copytree(self.output_dir, sd_tiles_dir, dirs_exist_ok=True)
                shutil.copy(self.gpx_path.get(), os.path.join(sd_folder, "route.gpx"))
                self.log("✅ Succesvol naar SD-kaart gekopieerd!")
            
        except Exception as e:
            self.log(f"❌ Error: {e}")
        finally:
            self.btn_process.config(state=tk.NORMAL)

    def latlon_to_tile(self, lat, lon, zoom):
        lat_rad = math.radians(lat)
        n = 2.0 ** zoom
        x = int((lon + 180.0) / 360.0 * n)
        y = int((1.0 - math.log(math.tan(lat_rad) + (1 / math.cos(lat_rad))) / math.pi) / 2.0 * n)
        return x, y

    def download_and_convert(self, lat_min, lat_max, lon_min, lon_max, out_dir):
        x_start, y_start = self.latlon_to_tile(lat_max, lon_min, ZOOM_LEVEL)
        x_end, y_end = self.latlon_to_tile(lat_min, lon_max, ZOOM_LEVEL)

        total_tiles = (x_end - x_start + 1) * (y_end - y_start + 1)
        self.log(f"Downloaden van {total_tiles} tiles...")

        for x in range(x_start, x_end + 1):
            for y in range(y_start, y_end + 1):
                url = f"https://tile.openstreetmap.org/{ZOOM_LEVEL}/{x}/{y}.png"
                folder = os.path.join(out_dir, str(ZOOM_LEVEL), str(x))
                os.makedirs(folder, exist_ok=True)
                target_path = os.path.join(folder, f"{y}.bin")

                if os.path.exists(target_path):
                    continue

                response = requests.get(url, headers={"User-Agent": USER_AGENT})
                if response.status_code == 200:
                    img = Image.open(BytesIO(response.content)).convert('RGB').resize((256, 256))
                    raw_data = bytearray()
                    for py in range(256):
                        for px in range(256):
                            r, g, b = img.getpixel((px, py))
                            rgb = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
                            raw_data.extend(struct.pack('>H', rgb))
                            
                    with open(target_path, "wb") as f:
                        f.write(raw_data)
                else:
                    self.log(f"Failed to fetch {url}")

    # --- TILE VIEWER LOGIC ---
    def open_tile_viewer(self):
        viewer = tk.Toplevel(self)
        viewer.title("RGB565 Tile Browser (240x320 ILI9341 Matcher)")
        viewer.geometry("600x450")

        left_frame = tk.Frame(viewer)
        left_frame.pack(side=tk.LEFT, fill=tk.Y, padx=10, pady=10)

        right_frame = tk.Frame(viewer)
        right_frame.pack(side=tk.RIGHT, fill=tk.BOTH, expand=True, padx=10, pady=10)

        tk.Label(left_frame, text="Gegenereerde .bin bestanden:").pack(anchor="w")
        listbox = tk.Listbox(left_frame, width=30)
        listbox.pack(fill=tk.Y, expand=True)

        canvas = tk.Canvas(right_frame, width=320, height=340, bg="gray")
        canvas.pack()
        
        tk.Label(right_frame, text="Rode kader = 240x320 ILI9341 Scherm\n(Tegels zijn standaard 256x256)", 
                 fg="red", font=("Arial", 9)).pack(pady=5)

        search_path = os.path.join(self.output_dir, "**", "*.bin")
        bin_files = glob.glob(search_path, recursive=True)
        for f in bin_files:
            listbox.insert(tk.END, os.path.relpath(f, self.output_dir))

        def on_select(evt):
            selection = listbox.curselection()
            if not selection: return
            
            file_rel = listbox.get(selection[0])
            full_path = os.path.join(self.output_dir, file_rel)
            
            img = self.decode_rgb565(full_path)
            
            photo = ImageTk.PhotoImage(img)
            canvas.image = photo 
            canvas.delete("all")
            # Centreer de 256x256 tegel in de 320x340 canvas
            canvas.create_image(160, 170, image=photo, anchor=tk.CENTER)
            
            # Teken de 240x320 portret modus kader (gecentreerd)
            # Center X = 160. Breedte 240 -> X van 40 tot 280
            # Center Y = 170. Hoogte 320 -> Y van 10 tot 330
            canvas.create_rectangle(40, 10, 280, 330, outline="red", width=2, dash=(4, 4))

        listbox.bind('<<ListboxSelect>>', on_select)

    def decode_rgb565(self, bin_file):
        with open(bin_file, "rb") as f:
            raw_data = f.read()
            
        img = Image.new('RGB', (256, 256))
        pixels = img.load()
        
        idx = 0
        for py in range(256):
            for px in range(256):
                if idx + 1 >= len(raw_data): break
                val = struct.unpack('>H', raw_data[idx:idx+2])[0]
                idx += 2
                
                r = (val >> 11) & 0x1F
                g = (val >> 5) & 0x3F
                b = val & 0x1F
                
                r = (r << 3) | (r >> 2)
                g = (g << 2) | (g >> 4)
                b = (b << 3) | (b >> 2)
                
                pixels[px, py] = (r, g, b)
        return img

if __name__ == "__main__":
    app = BikeComputerTool()
    app.mainloop()