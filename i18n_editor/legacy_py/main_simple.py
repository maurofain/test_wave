"""
i18n Editor Semplificato - Applicazione Flet per editare file i18n
"""
import flet as ft
from pathlib import Path
from i18n_manager import I18nManager


def main(page: ft.Page):
    page.title = "i18n Editor"
    page.window_width = 1200
    page.window_height = 800
    
    # Carica i dati
    data_dir = Path(__file__).parent.parent / "data"
    manager = I18nManager(str(data_dir))
    
    try:
        if not manager.load_all_files():
            page.add(ft.Text("❌ Errore: Nessun file i18n trovato in /data"))
            return
    except Exception as e:
        page.add(ft.Text(f"❌ Errore nel caricamento: {e}"))
        return
    
    # UI Components
    current_scope = [manager.scopes[0]] if manager.scopes else [None]
    search_results = []
    search_index = [0]
    
    def build_table():
        """Costruisce la tabella dei dati."""
        if not current_scope[0]:
            return ft.Column([ft.Text("Nessuno scope disponibile")])
        
        # Header
        header_cells = []
        header_cells.append(ft.Container(
            ft.Text("ID", weight="bold", color="white", size=12),
            width=60, padding=10, bgcolor="#1a1a1a"
        ))
        
        for lang in manager.languages:
            lang_name = manager.get_language_name(lang)
            header_cells.append(ft.Container(
                ft.Text(f"{lang_name}\n({lang})", 
                       weight="bold", color="white", size=10, 
                       text_align="center"),
                width=250, padding=10, bgcolor="#1a1a1a"
            ))
        
        rows = [ft.Row(header_cells, spacing=0)]
        
        # Data rows
        entries = manager.get_scope_data(current_scope[0])
        for entry in entries:
            key = entry['key']
            translations = entry.get('translations', {})
            
            cells = []
            cells.append(ft.Container(
                ft.Text(str(key), size=11, selectable=True),
                width=60, padding=8, border="all 1px #ccc"
            ))
            
            for lang in manager.languages:
                current_text = translations.get(lang, '')
                is_italian = lang == 'it'
                
                # TextBox edit
                def make_change_handler(s, k, l):
                    def on_change(e):
                        manager.update_translation(s, k, l, e.control.value)
                    return on_change
                
                tf = ft.TextField(
                    value=current_text,
                    multiline=True,
                    min_lines=1,
                    max_lines=3,
                    on_change=make_change_handler(current_scope[0], key, lang),
                    border_color="#0078d4" if is_italian else "#ccc",
                    border_width=2 if is_italian else 1
                )
                
                cells.append(ft.Container(
                    tf, width=250, padding=5, border="all 1px #ddd"
                ))
            
            rows.append(ft.Row(cells, spacing=0, scroll="auto"))
        
        return ft.Column(rows, spacing=0, scroll="auto", expand=True)
    
    def on_scope_change():
        """Quando lo scope cambia."""
        table.content = build_table()
        page.update()
    
    def on_search():
        """Ricerca di testo."""
        search_text = search_field.value.strip()
        if not search_text:
            status.value = "Inserisci un testo da cercare"
            page.update()
            return
        
        search_results.clear()
        search_results.extend(manager.search_all_scopes(search_text))
        
        if not search_results:
            status.value = f"Nessun risultato per: {search_text}"
            page.update()
            return
        
        search_index[0] = 0
        show_search_result()
    
    def show_search_result():
        """Mostra il risultato della ricerca corrente."""
        if not search_results or search_index[0] < 0 or search_index[0] >= len(search_results):
            return
        
        scope, key, lang, text = search_results[search_index[0]]
        current_scope[0] = scope
        on_scope_change()
        
        status.value = f"Risultato {search_index[0] + 1}/{len(search_results)}"
        page.update()
    
    def on_prev():
        """Risultato precedente."""
        if search_results:
            search_index[0] = (search_index[0] - 1) % len(search_results)
            show_search_result()
    
    def on_next():
        """Risultato successivo."""
        if search_results:
            search_index[0] = (search_index[0] + 1) % len(search_results)
            show_search_result()
    
    def on_save():
        """Salva i dati."""
        success, backup_file = manager.save_all_files()
        if success:
            dlg = ft.AlertDialog(ft.Text(f"✅ Salvato!\nBackup: {backup_file}"))
            page.dialog = dlg
            dlg.open = True
        else:
            dlg = ft.AlertDialog(ft.Text(f"❌ Errore: {backup_file}"))
            page.dialog = dlg
            dlg.open = True
        page.update()
    
    def on_exit():
        """Esce."""
        page.window_destroy()
    
    # UI Elements
    status = ft.Text(f"Lingue: {', '.join(manager.languages)}", size=11)
    
    search_field = ft.TextField(
        label="Ricerca", width=300,
        on_submit=lambda e: on_search()
    )
    
    scope_btn_minus = ft.IconButton(ft.icons.NAVIGATE_BEFORE, on_click=lambda e: (
        current_scope.pop() or current_scope.append(
            manager.scopes[max(0, manager.scopes.index(current_scope[0]) - 1)]
        ),
        on_scope_change()
    ) if current_scope[0] in manager.scopes else None)
    
    scope_label = ft.Text(f"Scope: {current_scope[0]}", width=100)
    
    scope_btn_plus = ft.IconButton(ft.icons.NAVIGATE_NEXT, on_click=lambda e: (
        current_scope.pop() or current_scope.append(
            manager.scopes[min(len(manager.scopes)-1, 
                             manager.scopes.index(current_scope[0]) + 1)]
        ),
        on_scope_change()
    ) if current_scope[0] in manager.scopes else None)
    
    # Top bar
    top_bar = ft.Row([
        ft.Text("Scope:", weight="bold"),
        scope_btn_minus, scope_label, scope_btn_plus,
        ft.VerticalDivider(width=1),
        ft.Text("Ricerca:", weight="bold"),
        search_field,
        ft.IconButton(ft.icons.SEARCH, on_click=lambda e: on_search()),
        ft.IconButton(ft.icons.NAVIGATE_BEFORE, on_click=lambda e: on_prev()),
        ft.IconButton(ft.icons.NAVIGATE_NEXT, on_click=lambda e: on_next()),
        ft.Container(expand=True),
        ft.ElevatedButton("💾 SALVA", on_click=lambda e: on_save()),
        ft.ElevatedButton("🚪 ESCI", on_click=lambda e: on_exit()),
    ], wrap=False, scroll="auto", spacing=10, padding=10)
    
    # Data table
    table = ft.Container(
        content=build_table(),
        expand=True,
        border="all 1px #ddd"
    )
    
    # Footer
    footer = ft.Container(
        content=status,
        padding=10,
        bgcolor="#f0f0f0",
        border="top 1px #ddd"
    )
    
    # Main layout
    page.add(
        ft.Column([
            top_bar,
            table,
            footer
        ], expand=True, spacing=0)
    )


if __name__ == "__main__":
    ft.run(main)
