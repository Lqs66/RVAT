import os
import sqlite3
from .utils import *

'''
    Database Table Structure
    +-----------------------------+
    | properties                  |
    +-----------------------------+
    | id: BIGINT (PK, auto-increment) |  -- Primary key, unique identifier for property record
    | property: VARCHAR(255)      |  -- Target property
    | callee_id: uint32_t         |  -- Callee function ID (fid or iid)
    | is_direct: BOOLEAN          |  -- TRUE=direct call, FALSE=indirect call
    +-----------------------------+
            |
            | 1:N (one-to-many)
            |
            V
    +-----------------------------+
    | return_values               |
    +-----------------------------+
    | id: BIGINT (PK, auto-increment) |  -- Primary key, unique identifier for return value record
    | pid: BIGINT (FK)            |  -- Foreign key, references properties.id
    | return_value: BLOB          |  -- Single return value
    +-----------------------------+

    Sample Data
    properties table
    id | callee_id  | is_direct | return_values
    1  | AAA        | TRUE      | ["val1", "val2"]
    2  | BBB        | FALSE     | ["val3"]
'''

def create_database(db_path: str):
    db_dir = os.path.dirname(os.path.abspath(db_path))
    if db_dir and not os.path.exists(db_dir):
        os.makedirs(db_dir, exist_ok=True)

    try:
        with sqlite3.connect(db_path) as conn:
            cursor = conn.cursor()

            # Create properties table
            cursor.execute('''
                CREATE TABLE IF NOT EXISTS properties (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    property TEXT NOT NULL,
                    callee_id INTEGER NOT NULL CHECK (callee_id >= 0),
                    is_direct BOOLEAN NOT NULL,
                    UNIQUE (property, callee_id, is_direct)
                )
            ''')

            # Create return_values table
            cursor.execute('''
                CREATE TABLE IF NOT EXISTS return_values (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    pid INTEGER NOT NULL,
                    return_value BLOB NOT NULL,
                    FOREIGN KEY (pid) REFERENCES properties(id) ON DELETE CASCADE,
                    UNIQUE (pid, return_value)
                )
            ''')

            conn.commit()
    except sqlite3.OperationalError as exc:
        ERROR(f"Failed to create or open database at {db_path}: {exc}")
        raise
    else:
        INFO('Database and tables created successfully.')

def insert_property(conn, property: str, callee_id: int, is_direct: bool):
    cursor = conn.cursor()
    try:
        cursor.execute('''
            INSERT INTO properties (property, callee_id, is_direct)
            VALUES (?, ?, ?)
        ''', (property, callee_id, is_direct))
        return cursor.lastrowid
    except sqlite3.IntegrityError:
        return None

def insert_return_value(conn, pid: int, return_value: bytes):
    cursor = conn.cursor()
    try:
        cursor.execute('''
            INSERT INTO return_values (pid, return_value)
            VALUES (?, ?)
        ''', (pid, return_value))
        return cursor.lastrowid
    except sqlite3.IntegrityError:
        return None

if __name__ == "__main__":
    create_database('funcSum.db')