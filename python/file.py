import sqlite3

# Connexion à la base (elle sera créée si elle n’existe pas)
conn = sqlite3.connect('authorized_plates.db')
c = conn.cursor()

# Création de la table des plaques autorisées
c.execute('CREATE TABLE IF NOT EXISTS authorized (plate_text TEXT UNIQUE)')

# Insertion de quelques plaques d'exemple
c.executemany('INSERT OR IGNORE INTO authorized (plate_text) VALUES (?)', [
    ('E43717BT',),
    ('456XYZ',),
    ('45781Y126',)
])

conn.commit()
conn.close()
