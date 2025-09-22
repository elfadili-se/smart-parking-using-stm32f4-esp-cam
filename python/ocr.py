import easyocr
import sqlite3
import sys
from datetime import datetime

# Récupère le chemin de l'image envoyé en argument
if len(sys.argv) < 2:
    print("No image path provided")
    exit(1)

image_path = sys.argv[1]
reader = easyocr.Reader(['en'])

# Lecture OCR
results = reader.readtext(image_path)

if not results:
    print("No plate detected")
    exit(1)

# Prend la première valeur détectée
plate = results[0][1].strip().upper().replace(" ", "")
print("Detected plate:", plate)

# Connexion à la base
conn = sqlite3.connect("parking.db")
cursor = conn.cursor()

# Vérifie si la plaque est autorisée
cursor.execute("SELECT * FROM whitelist WHERE plate = ?", (plate,))
authorized = cursor.fetchone()

if authorized:
    now = datetime.now()
    cursor.execute("SELECT * FROM history WHERE plate=? AND exit_time IS NULL", (plate,))
    existing_entry = cursor.fetchone()

    if existing_entry:
        # Voiture sort
        cursor.execute("UPDATE history SET exit_time = ? WHERE id = ?", (now, existing_entry[0]))
        status = "EXIT"
    else:
        # Voiture entre
        cursor.execute("INSERT INTO history (plate, entry_time) VALUES (?, ?)", (plate, now))
        status = "ENTRY"

    conn.commit()
    print(f"{plate}:{status}")
else:
    print(f"{plate}:DENIED")

conn.close()
