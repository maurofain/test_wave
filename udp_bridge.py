#!/usr/bin/env python3
import socket
import requests
import json
from datetime import datetime

# Configurazione
UDP_IP = "0.0.0.0"      # Ascolta su tutte le interfacce
UDP_PORT = 9514         # Porta UDP per ricevere log
ESP32_IP = "192.168.2.16"  # IP del tuo ESP32 (!!! CAMBIA QUESTO !!!)
ESP32_PORT = 80         # Porta HTTP ESP32 (default 80)

# Trova automaticamente l'IP ESP32 (opzionale)
def find_esp32_ip():
    # Cerca dispositivi in rete con hostname contenente "esp32" o "test_wave"
    # Per ora usa l'IP configurato
    return ESP32_IP

def send_to_esp32(level, tag, message):
    """Invia log all'ESP32 via HTTP POST"""
    try:
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        payload = {
            "level": level,
            "tag": tag, 
            "message": message,
            "timestamp": timestamp
        }
        
        url = f"http://{ESP32_IP}:{ESP32_PORT}/api/logs/receive"
        response = requests.post(url, json=payload, timeout=2)
        
        if response.status_code == 200:
            print(f"✅ Inviato a ESP32: {message[:50]}...")
        else:
            print(f"❌ Errore ESP32: {response.status_code}")
            
    except Exception as e:
        print(f"❌ Errore invio ESP32: {e}")

def main():
    # Crea socket UDP
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    
    try:
        sock.bind((UDP_IP, UDP_PORT))
        print(f"🎧 Bridge UDP attivo su porta {UDP_PORT}")
        print(f"📡 Invio log a ESP32: {ESP32_IP}:{ESP32_PORT}")
        print("Premi Ctrl+C per uscire")
        
        while True:
            data, addr = sock.recvfrom(2048)  # Buffer più grande
            try:
                log_line = data.decode('utf-8', errors='ignore').strip()
                
                # Parsing del log ESP-IDF (formato: [timestamp] level tag: message)
                if log_line.startswith('[') and ']' in log_line:
                    # È un log formattato dall'ESP32
                    parts = log_line.split('] ', 2)
                    if len(parts) >= 3:
                        timestamp_part = parts[0] + ']'
                        level_tag = parts[1]
                        message = parts[2]
                        
                        # Estrai level e tag
                        level_tag_parts = level_tag.split(' ', 1)
                        if len(level_tag_parts) >= 2:
                            level = level_tag_parts[0]
                            tag_message = level_tag_parts[1]
                            
                            if ':' in tag_message:
                                tag, message = tag_message.split(':', 1)
                                tag = tag.strip()
                                message = message.strip()
                            else:
                                tag = "UNKNOWN"
                        else:
                            level = "INFO"
                            tag = "UNKNOWN"
                            message = level_tag
                        
                        print(f"📨 {addr[0]} → {level} {tag}: {message}")
                        send_to_esp32(level, tag, message)
                    else:
                        # Log non formattato
                        print(f"📨 {addr[0]} → {log_line}")
                        send_to_esp32("INFO", "REMOTE", log_line)
                else:
                    # Log semplice
                    print(f"📨 {addr[0]} → {log_line}")
                    send_to_esp32("INFO", "REMOTE", log_line)
                    
            except UnicodeDecodeError:
                print(f"📨 {addr[0]} → [Binary data received]")
                
    except KeyboardInterrupt:
        print("\n👋 Chiusura bridge...")
    except Exception as e:
        print(f"❌ Errore: {e}")
    finally:
        sock.close()

if __name__ == "__main__":
    main()
