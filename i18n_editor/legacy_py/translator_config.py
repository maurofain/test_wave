"""
Configurazione per i servizi di traduzione automatica.
"""
import json
from pathlib import Path

class TranslatorConfig:
    """Gestisce la configurazione per i servizi di traduzione."""
    
    # Configurazione predefinita con OpenAI API
    DEFAULT_CONFIG = {
        "enabled": True,
        "service": "openai",  # openai | google | deepl | ollama
        "openai": {
            "api_key": "",  # Impostare da variabile ambiente
            "model": "gpt-3.5-turbo",
            "temperature": 0.3
        },
        "google": {
            "api_key": "",
            "project_id": ""
        },
        "deepl": {
            "api_key": "",
            "api_type": "free"  # free | pro
        },
        "ollama": {
            "base_url": "http://localhost:11434",
            "model": "mistral"
        }
    }
    
    def __init__(self, config_file: str = None):
        """
        Inizializza la configurazione.
        
        Args:
            config_file: Path al file di configurazione JSON (opzionale)
        """
        self.config = self.DEFAULT_CONFIG.copy()
        self.config_file = Path(config_file) if config_file else None
        
        if self.config_file and self.config_file.exists():
            self.load_config()
    
    def load_config(self):
        """Carica la configurazione da file JSON."""
        try:
            with open(self.config_file, 'r', encoding='utf-8') as f:
                loaded = json.load(f)
                # Merge con i default
                self._deep_merge(self.config, loaded)
        except Exception as e:
            print(f"Errore nel caricamento configurazione: {e}")
    
    def save_config(self):
        """Salva la configurazione su file JSON."""
        if not self.config_file:
            raise ValueError("No config file specified")
        
        try:
            self.config_file.parent.mkdir(parents=True, exist_ok=True)
            with open(self.config_file, 'w', encoding='utf-8') as f:
                json.dump(self.config, f, indent=2, ensure_ascii=False)
        except Exception as e:
            print(f"Errore nel salvataggio configurazione: {e}")
    
    def _deep_merge(self, base: dict, override: dict):
        """Merges override dict into base dict (in-place)."""
        for key, value in override.items():
            if key in base and isinstance(base[key], dict) and isinstance(value, dict):
                self._deep_merge(base[key], value)
            else:
                base[key] = value
    
    def get_service(self) -> str:
        """Ritorna il servizio di traduzione configurato."""
        return self.config.get('service', 'openai')
    
    def is_enabled(self) -> bool:
        """Ritorna True se la traduzione automatica è abilitata."""
        return self.config.get('enabled', False)
    
    def set_enabled(self, enabled: bool):
        """Abilita/disabilita la traduzione automatica."""
        self.config['enabled'] = enabled
        if self.config_file:
            self.save_config()


class TranslatorService:
    """Base class per i servizi di traduzione."""
    
    def __init__(self, config: TranslatorConfig):
        self.config = config
    
    async def translate(self, text: str, source_lang: str, target_lang: str) -> str:
        """
        Traduce un testo.
        
        Args:
            text: Testo da tradurre
            source_lang: Codice lingua sorgente (es. 'it')
            target_lang: Codice lingua destinazione (es. 'en')
        
        Returns:
            Testo tradotto
        """
        raise NotImplementedError


class OpenAITranslator(TranslatorService):
    """Servizio di traduzione con OpenAI API."""
    
    def __init__(self, config: TranslatorConfig):
        super().__init__(config)
        self.api_key = config.config['openai'].get('api_key')
        self.model = config.config['openai'].get('model', 'gpt-3.5-turbo')
    
    async def translate(self, text: str, source_lang: str, target_lang: str) -> str:
        """Traduce usando OpenAI API."""
        if not self.api_key:
            raise ValueError("OpenAI API key not configured")
        
        try:
            import openai
            openai.api_key = self.api_key
            
            # Mappa codici lingua a nomi
            lang_names = {
                'it': 'Italian',
                'en': 'English', 
                'de': 'German',
                'es': 'Spanish',
                'fr': 'French'
            }
            
            source_name = lang_names.get(source_lang, source_lang)
            target_name = lang_names.get(target_lang, target_lang)
            
            prompt = f"""Traduci il seguente testo da {source_name} a {target_name}.
            Rispondi SOLO con il testo tradotto, senza altre aggiunte.
            
            Testo: {text}"""
            
            response = await openai.ChatCompletion.acreate(
                model=self.model,
                messages=[{"role": "user", "content": prompt}],
                temperature=0.3
            )
            
            return response.choices[0].message.content.strip()
        
        except Exception as e:
            raise RuntimeError(f"Errore traduzione OpenAI: {e}")


class OllamaTranslator(TranslatorService):
    """Servizio di traduzione locale con Ollama."""
    
    async def translate(self, text: str, source_lang: str, target_lang: str) -> str:
        """Traduce usando Ollama localmente."""
        try:
            import httpx
            
            base_url = self.config.config['ollama'].get('base_url', 'http://localhost:11434')
            model = self.config.config['ollama'].get('model', 'mistral')
            
            lang_names = {
                'it': 'Italian',
                'en': 'English',
                'de': 'German',
                'es': 'Spanish',
                'fr': 'French'
            }
            
            source_name = lang_names.get(source_lang, source_lang)
            target_name = lang_names.get(target_lang, target_lang)
            
            prompt = f"""Traduci il seguente testo da {source_name} a {target_name}.
            Rispondi SOLO con il testo tradotto, senza altre aggiunte.
            
            Testo: {text}"""
            
            async with httpx.AsyncClient() as client:
                response = await client.post(
                    f"{base_url}/api/generate",
                    json={"model": model, "prompt": prompt, "stream": False}
                )
                
                if response.status_code == 200:
                    result = response.json()
                    return result.get('response', '').strip()
                else:
                    raise RuntimeError(f"Ollama error: {response.status_code}")
        
        except Exception as e:
            raise RuntimeError(f"Errore traduzione Ollama: {e}")


def get_translator(config: TranslatorConfig) -> TranslatorService:
    """Factory function per ottenere il traduttore configurato."""
    service = config.get_service()
    
    if service == 'openai':
        return OpenAITranslator(config)
    elif service == 'ollama':
        return OllamaTranslator(config)
    else:
        raise ValueError(f"Unsupported translation service: {service}")
