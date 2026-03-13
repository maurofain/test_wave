"""
Gestore file i18n per l'editor multilingue.
Legge, organizza e salva i file di localizzazione JSON.
"""
import json
import re
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Tuple, Any

class I18nManager:
    """Gestione dei file di localizzazione i18n_xx.json"""
    
    # Ordine delle lingue: Italian first, others in alpha order
    LANGUAGE_ORDER = {
        'it': 0,  # Italian first
        'en': 1,  # English second
        'de': 2,  # German third
        'es': 3,  # Spanish  
        'fr': 4,  # French
    }
    
    LANGUAGE_NAMES = {
        'it': 'Italiano',
        'en': 'English',
        'de': 'Deutsch',
        'es': 'Español',
        'fr': 'Français',
    }
    
    def __init__(self, data_dir: str):
        """
        Inizializza il manager con la directory dei file i18n.
        
        Args:
            data_dir: Percorso alla cartella /data con i file i18n_xx.json
        """
        self.data_dir = Path(data_dir)
        self.i18n_data: Dict[str, List[Dict]] = {}  # {lang: [entries]}
        self.languages: List[str] = []
        self.entries_by_scope: Dict[int, List[Dict]] = {}  # {scope: [entries]}
        self.scopes: List[int] = []
        self.scope_name_map: Dict[int, str] = {}
        self.key_name_map: Dict[int, str] = {}
        self.map_loaded: bool = False

    def load_id_name_map(self, map_file_path: str) -> bool:
        """
        Carica la mappa di decodifica ID (scope/key -> nome leggibile).

        Args:
            map_file_path: Percorso al file docs/i18n/i18n_it.map.json

        Returns:
            True se caricata correttamente, False altrimenti
        """
        self.scope_name_map = {}
        self.key_name_map = {}
        self.map_loaded = False

        map_path = Path(map_file_path)
        if not map_path.exists():
            return False

        try:
            with open(map_path, 'r', encoding='utf-8') as file:
                data = json.load(file)

            for scope_id, scope_name in data.get('scopes', {}).items():
                try:
                    self.scope_name_map[int(scope_id)] = str(scope_name)
                except (TypeError, ValueError):
                    continue

            for key_id, key_name in data.get('keys', {}).items():
                try:
                    self.key_name_map[int(key_id)] = str(key_name)
                except (TypeError, ValueError):
                    continue

            self.map_loaded = True
            return True
        except Exception:
            return False

    def get_scope_name(self, scope_id: int) -> str:
        """Ritorna il nome decodificato dello scope, se disponibile."""
        return self.scope_name_map.get(scope_id, "")

    def get_key_name(self, key_id: int) -> str:
        """Ritorna il nome decodificato della key, se disponibile."""
        return self.key_name_map.get(key_id, "")

    def get_scope_label(self, scope_id: int) -> str:
        """Ritorna una label leggibile per lo scope (ID + nome)."""
        scope_name = self.get_scope_name(scope_id)
        if scope_name:
            return f"Scope {scope_id} · {scope_name}"
        return f"Scope {scope_id}"
        
    def load_all_files(self) -> bool:
        """
        Carica tutti i file i18n_xx.json dalla directory.
        
        Returns:
            True se almeno un file è stato caricato
        """
        self.i18n_data = {}
        
        if not self.data_dir.exists():
            raise FileNotFoundError(f"Directory not found: {self.data_dir}")
        
        # Trova tutti i file i18n_xx.json (solo codici lingua a 2 lettere)
        lang_file_pattern = re.compile(r"^i18n_([a-z]{2})\.json$", re.IGNORECASE)
        for file_path in sorted(self.data_dir.glob('i18n_*.json')):
            match = lang_file_pattern.match(file_path.name)
            if not match:
                continue

            lang_code = match.group(1).lower()
            
            try:
                with open(file_path, 'r', encoding='utf-8') as f:
                    data = json.load(f)
                    if isinstance(data, list):
                        self.i18n_data[lang_code] = data
            except Exception as e:
                print(f"Errore nel caricamento {file_path}: {e}")
                continue
        
        if not self.i18n_data:
            return False
        
        # Ordina le lingue secondo LANGUAGE_ORDER
        self.languages = sorted(
            self.i18n_data.keys(),
            key=lambda x: self.LANGUAGE_ORDER.get(x, 999)
        )
        
        # Costruisce indice per scope
        self._build_scope_index()
        return True
    
    def _build_scope_index(self):
        """Costruisce un indice di scope da tutte le voci caricate."""
        self.scopes = self._extract_unique_scopes_from_reference()
        self.entries_by_scope = {scope: [] for scope in self.scopes}
        
        # Popola le voci ordinate per scope
        for scope in self.scopes:
            # Crea una mappa di tutte le voci per (key, section) da tutte le lingue
            key_to_entry: Dict[Tuple[int, int], Dict[str, Any]] = {}
            
            for lang in self.languages:
                for entry in self.i18n_data[lang]:
                    if entry.get('scope') == scope:
                        key = entry.get('key')
                        if key is None:
                            continue

                        section = entry.get('section', 0)
                        if section is None:
                            section = 0

                        entry_id = (key, section)
                        if entry_id not in key_to_entry:
                            key_to_entry[entry_id] = {
                                'key': key,
                                'section': section,
                                'scope': scope,
                                'translations': {}
                            }
                        key_to_entry[entry_id]['translations'][lang] = entry.get('text', '')
            
            # Converte in lista, ordinata per key
            entries_list = sorted(
                key_to_entry.values(),
                key=lambda x: (x['key'], x.get('section', 0))
            )
            self.entries_by_scope[scope] = entries_list

    def _extract_unique_scopes_from_reference(self) -> List[int]:
        """Estrae gli scope unici dal file italiano (fallback su prima lingua disponibile)."""
        reference_lang = 'it' if 'it' in self.i18n_data else (self.languages[0] if self.languages else None)
        if not reference_lang:
            return []

        unique_scopes = {
            entry.get('scope')
            for entry in self.i18n_data.get(reference_lang, [])
            if entry.get('scope') is not None
        }

        return sorted(unique_scopes)
    
    def get_scope_data(self, scope: int) -> List[Dict]:
        """
        Ritorna tutte le voci di uno scope con il testo per tutte le lingue.
        
        Returns:
            Lista di dizionari con struttura:
            {
                'key': int,
                'section': int,
                'scope': int,
                'translations': {'it': 'testo', 'en': 'text', ...}
            }
        """
        return self.entries_by_scope.get(scope, [])
    
    def update_translation(self, scope: int, key: int, section: int, lang: str, text: str):
        """
        Aggiorna una traduzione nel dati in memoria.
        
        Args:
            scope: ID dello scope
            key: ID della chiave
            section: ID della sezione
            lang: Codice lingua (es. 'it', 'en')
            text: Nuovo testo della traduzione
        """
        if lang not in self.i18n_data:
            return False
        
        # Trova e aggiorna l'entry
        for entry in self.i18n_data[lang]:
            entry_section = entry.get('section', 0)
            if entry_section is None:
                entry_section = 0

            if entry.get('scope') == scope and entry.get('key') == key and entry_section == section:
                entry['text'] = text
                return True
        
        return False
    
    def save_all_files(self) -> Tuple[bool, str]:
        """
        Salva tutti i dati in memoria nei file i18n_xx.json
        e crea un backup con timestamp.
        
        Returns:
            Tuple (success: bool, backup_file: str)
        """
        # Crea backup con timestamp
        timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
        backup_filename = f"i18n_{timestamp}.json"
        backup_path = self.data_dir / backup_filename
        
        # Salva backup (mantiene tutte le lingue separate)
        try:
            backup_data = {
                'timestamp': timestamp,
                'languages': self.languages,
                'data': {lang: self.i18n_data.get(lang, []) for lang in self.languages},
            }
            
            with open(backup_path, 'w', encoding='utf-8') as f:
                json.dump(backup_data, f, ensure_ascii=False, indent=2)
        except Exception as e:
            return False, f"Errore nel salvataggio backup: {e}"
        
        # Salva i file singoli per lingua
        try:
            for lang in self.languages:
                file_path = self.data_dir / f"i18n_{lang}.json"
                with open(file_path, 'w', encoding='utf-8') as f:
                    json.dump(self.i18n_data[lang], f, ensure_ascii=False, indent=0)
        except Exception as e:
            return False, f"Errore nel salvataggio file lingue: {e}"
        
        return True, backup_filename
    
    def search_all_scopes(self, search_text: str) -> List[Tuple[int, int, int, str, str]]:
        """
        Ricerca una stringa in tutti gli scope e tutte le lingue.
        
        Args:
            search_text: Testo da cercare (case-insensitive)
        
        Returns:
            Lista di tuple (scope, key, section, lang, text)
        """
        results = []
        search_lower = search_text.lower()
        
        for scope in self.scopes:
            entries = self.get_scope_data(scope)
            for entry in entries:
                key = entry['key']
                section = entry.get('section', 0)
                for lang, text in entry.get('translations', {}).items():
                    if search_lower in text.lower():
                        results.append((scope, key, section, lang, text))
        
        return results
    
    def get_language_name(self, lang_code: str) -> str:
        """Ritorna il nome della lingua in italiano."""
        return self.LANGUAGE_NAMES.get(lang_code, lang_code.upper())
