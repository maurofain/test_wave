"""
Utilità per validare e analizzare i file i18n.
Può essere eseguito dalla riga di comando.
"""
import json
from pathlib import Path
from i18n_manager import I18nManager
from collections import defaultdict


class I18nValidator:
    """Valida e analizza i file i18n."""
    
    def __init__(self, manager: I18nManager):
        self.manager = manager
    
    def validate_all(self) -> dict:
        """
        Esegue una validazione completa dei dati.
        
        Returns:
            Dict con risultati della validazione
        """
        results = {
            'valid': True,
            'errors': [],
            'warnings': [],
            'stats': {}
        }
        
        # Verifica che tutte le lingue abbiano lo stesso numero di scope
        scope_counts = defaultdict(int)
        for lang in self.manager.languages:
            for entry in self.manager.i18n_data[lang]:
                scope = entry.get('scope')
                scope_counts[(lang, scope)] += 1
        
        # Analizza statistiche
        results['stats'] = {
            'languages': len(self.manager.languages),
            'scopes': len(self.manager.scopes),
            'total_entries': sum(len(e) for e in self.manager.i18n_data.values())
        }
        
        # Controlla che ogni scope abbia le stesse chiavi in tutte le lingue
        for scope in self.manager.scopes:
            keys_per_lang = {}
            
            for lang in self.manager.languages:
                keys = set()
                for entry in self.manager.i18n_data[lang]:
                    if entry.get('scope') == scope:
                        keys.add((entry.get('key'), entry.get('section')))
                keys_per_lang[lang] = keys
            
            # Controlla coerenza
            reference_keys = keys_per_lang.get(self.manager.languages[0], set())
            for lang in self.manager.languages[1:]:
                missing = reference_keys - keys_per_lang[lang]
                extra = keys_per_lang[lang] - reference_keys
                
                if missing:
                    results['errors'].append(
                        f"Scope {scope}: {lang} mancano keys {missing}"
                    )
                    results['valid'] = False
                
                if extra:
                    results['warnings'].append(
                        f"Scope {scope}: {lang} ha keys extra {extra}"
                    )
        
        # Controlla testi vuoti
        for lang in self.manager.languages:
            empty_count = 0
            for entry in self.manager.i18n_data[lang]:
                if not entry.get('text', '').strip():
                    empty_count += 1
            
            if empty_count > 0:
                results['warnings'].append(
                    f"Lingua {lang}: {empty_count} testi vuoti"
                )
        
        return results
    
    def print_stats(self):
        """Stampa statistiche sui file caricati."""
        print("\n" + "="*60)
        print("STATISTICHE I18N")
        print("="*60)
        
        print(f"\nLingue caricate: {len(self.manager.languages)}")
        for lang in self.manager.languages:
            count = len(self.manager.i18n_data.get(lang, []))
            print(f"  - {lang}: {count} voci")
        
        print(f"\nScope disponibili: {len(self.manager.scopes)}")
        for scope in self.manager.scopes[:10]:  # Mostra i primi 10
            entries = self.manager.get_scope_data(scope)
            print(f"  - Scope {scope}: {len(entries)} chiavi")
        
        if len(self.manager.scopes) > 10:
            print(f"  ... e {len(self.manager.scopes) - 10} altri scope")
        
        print(f"\nTotal keys: {sum(len(e) for e in self.manager.i18n_data.values())}")
    
    def print_validation(self):
        """Stampa risultati della validazione."""
        results = self.validate_all()
        
        print("\n" + "="*60)
        print("VALIDAZIONE")
        print("="*60)
        
        if results['valid']:
            print("✅ Validazione SUPERATA")
        else:
            print("❌ Errori rilevati:")
            for error in results['errors']:
                print(f"  ERROR: {error}")
        
        if results['warnings']:
            print("\n⚠️ Avvertenze:")
            for warning in results['warnings']:
                print(f"  WARNING: {warning}")
        
        if not results['errors'] and not results['warnings']:
            print("\n✅ Nessun errore o avvertenza!")


def main():
    """Funzione principale per uso CLI."""
    import sys
    
    # Controlla argomenti
    data_dir = Path(__file__).parent.parent / "data"
    
    if not data_dir.exists():
        print(f"Errore: Directory {data_dir} non trovata")
        sys.exit(1)
    
    print(f"Caricamento da: {data_dir}")
    
    # Carica i dati
    manager = I18nManager(str(data_dir))
    if not manager.load_all_files():
        print("Errore: Nessun file i18n trovato")
        sys.exit(1)
    
    # Crea validatore
    validator = I18nValidator(manager)
    
    # Stampa risultati
    validator.print_stats()
    validator.print_validation()
    
    print("\n" + "="*60 + "\n")


if __name__ == "__main__":
    main()
