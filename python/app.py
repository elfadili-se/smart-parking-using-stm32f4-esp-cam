# ---------------------- Importation des bibliothèques nécessaires ----------------------
import os
import cv2
import sqlite3
import logging
from datetime import datetime
from flask import Flask, request, jsonify
import easyocr

# ---------------------- Configuration du système de logs ----------------------
logging.basicConfig(level=logging.INFO, format='[%(asctime)s] %(levelname)s - %(message)s')

# ---------------------- Initialisation de Flask et des répertoires ----------------------
app = Flask(__name__)
UPLOAD_FOLDER = "images"
os.makedirs(UPLOAD_FOLDER, exist_ok=True)

# ---------------------- Initialisation du lecteur OCR ----------------------
ocr_reader = easyocr.Reader(['en'])

# ---------------------- Chemins des bases de données ----------------------
AUTHORIZED_DB = "authorized_plates.db"
HISTORY_DB = "entry_history.db"

# ---------------------- Initialisation des bases de données ----------------------
def init_history_db():
    conn = sqlite3.connect(HISTORY_DB)
    c = conn.cursor()
    c.execute('''
        CREATE TABLE IF NOT EXISTS history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            plate_text TEXT,
            status TEXT,
            date TEXT
        )
    ''')
    conn.commit()
    conn.close()
    logging.info("Base de données 'history' initialisée.")

def init_authorized_db():
    conn = sqlite3.connect(AUTHORIZED_DB)
    c = conn.cursor()
    c.execute('''
        CREATE TABLE IF NOT EXISTS authorized (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            plate_text TEXT UNIQUE
        )
    ''')
    conn.commit()
    conn.close()
    logging.info("Base de données 'authorized' initialisée.")

init_history_db()
init_authorized_db()

# ---------------------- Vérification d'autorisation ----------------------
def is_plate_authorized(plate_text):
    conn = sqlite3.connect(AUTHORIZED_DB)
    c = conn.cursor()
    c.execute("SELECT 1 FROM authorized WHERE plate_text = ?", (plate_text,))
    result = c.fetchone()
    conn.close()
    return result is not None

# ---------------------- Sauvegarde dans l'historique ----------------------
def save_to_history(plate_text, status):
    conn = sqlite3.connect(HISTORY_DB)
    c = conn.cursor()
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    c.execute("INSERT INTO history (plate_text, status, date) VALUES (?, ?, ?)", 
              (plate_text, status, timestamp))
    conn.commit()
    conn.close()
    logging.info(f"[{status.upper()}] Plaque '{plate_text}' enregistrée à {timestamp}")

# ---------------------- Route principale pour réception image ----------------------
@app.route('/upload', methods=['POST'])
def upload_image():
    if 'image' not in request.files:
        logging.warning("Aucune image reçue.")
        return jsonify({'error': 'No image uploaded'}), 400

    file = request.files['image']
    filename = datetime.now().strftime("plate_%Y%m%d_%H%M%S.jpg")
    filepath = os.path.join(UPLOAD_FOLDER, filename)
    file.save(filepath)
    logging.info(f"Image reçue et sauvegardée : {filepath}")

    # Lecture de l'image et OCR
    image = cv2.imread(filepath)
    if image is None:
        logging.error("Image non lisible par OpenCV.")
        return jsonify({'error': 'Invalid image file'}), 400

    results = ocr_reader.readtext(image)

    if not results:
        logging.warning("Aucun texte détecté dans l’image.")
        return jsonify({'error': 'No text found in image'}), 400

    # Prend le premier texte trouvé et nettoie
    detected_text = results[0][1].upper().replace(" ", "").strip()
    logging.info(f"Texte détecté : {detected_text}")

    # Vérifie si plaque autorisée
    is_auth = is_plate_authorized(detected_text)
    status = "authorized" if is_auth else "unauthorized"

    # Sauvegarde dans l'historique
    save_to_history(detected_text, status)

    # Envoie de la réponse au client (ESP32-CAM)
    return "valid car" if is_auth else "invalid car"

# ---------------------- Lancement du serveur Flask ----------------------
if __name__ == '__main__':
    logging.info("Démarrage du serveur Flask sur http://0.0.0.0:5000")
    app.run(host='0.0.0.0', port=5000)
