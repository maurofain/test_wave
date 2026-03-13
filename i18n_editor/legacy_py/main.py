"""
i18n Editor - Applicazione Flet per editare i file di localizzazione multilingue.
"""
import flet as ft
import sys
import os
from pathlib import Path
from i18n_manager import I18nManager
from translator_config import TranslatorConfig

# Aggiungi il parent directory al path per importare i moduli
sys.path.insert(0, str(Path(__file__).parent))

# Alias per compatibility con le versioni di Flet
if not hasattr(ft, 'colors'):
    ft.colors = ft.Colors

if not hasattr(ft.icons, 'ARROW_BACK') and hasattr(ft, 'Icons'):
    ft.icons = ft.Icons


class I18nEditorApp:
    """Applicazione principale per editare i file i18n."""

    LIGHT_PALETTE = {
        "app_bg": "#F1F5F9",
        "panel_bg": "#FFFFFF",
        "header_bg": "#1E3A8A",
        "header_text": "#FFFFFF",
        "table_header_bg": "#1F2937",
        "table_header_text": "#F8FAFC",
        "table_meta_bg": "#E2E8F0",
        "row_bg_even": "#FFFFFF",
        "row_bg_odd": "#F8FAFC",
        "input_bg": "#FFFFFF",
        "text_primary": "#0F172A",
        "text_secondary": "#334155",
        "border": "#CBD5E1",
        "accent": "#2563EB",
        "success": "#15803D",
        "danger": "#B91C1C",
        "highlight": "#F59E0B",
    }

    DARK_PALETTE = {
        "app_bg": "#0B1220",
        "panel_bg": "#111827",
        "header_bg": "#0F172A",
        "header_text": "#E5E7EB",
        "table_header_bg": "#1F2937",
        "table_header_text": "#E5E7EB",
        "table_meta_bg": "#172033",
        "row_bg_even": "#111827",
        "row_bg_odd": "#0F172A",
        "input_bg": "#0B1220",
        "text_primary": "#E5E7EB",
        "text_secondary": "#9CA3AF",
        "border": "#334155",
        "accent": "#60A5FA",
        "success": "#22C55E",
        "danger": "#EF4444",
        "highlight": "#FBBF24",
    }
    
    def __init__(self, page: ft.Page):
        self.page = page
        self.page.title = "i18n Editor - Gestore Localizzazioni"
        self.page.window_width = 1400
        self.page.window_height = 800
        self.page.theme_mode = ft.ThemeMode.LIGHT
        self.palette = self.LIGHT_PALETTE
        
        # Inizializza il manager i18n
        data_dir = Path(__file__).parent.parent / "data"
        self.manager = I18nManager(str(data_dir))
        map_file = Path(__file__).parent.parent / "docs" / "i18n" / "i18n_it.map.json"
        self.manager.load_id_name_map(str(map_file))
        
        # Carica configurazione traduttore
        config_file = Path(__file__).parent / "translator_config.json"
        self.translator_config = TranslatorConfig(str(config_file))
        
        # Stato dell'applicazione
        self.current_scope: int = None
        self.search_results = []
        self.search_index = 0
        self.has_changes = False
        self.edit_fields = {}  # {(scope, key, section, lang): TextField}
        
        self._init_ui()

    def _get_palette(self):
        """Ritorna la palette coerente col tema corrente."""
        if self.page.theme_mode == ft.ThemeMode.DARK:
            return self.DARK_PALETTE
        return self.LIGHT_PALETTE

    def _update_scope_entries_info(self):
        """Aggiorna info scope e numero campi visualizzati."""
        if not hasattr(self, 'scope_entries_info'):
            return

        if not self.current_scope:
            self.scope_entries_info.value = "Nessuno scope selezionato"
            return

        total_entries = len(self.manager.get_scope_data(self.current_scope))
        self.scope_entries_info.value = f"{self.manager.get_scope_label(self.current_scope)} · campi: {total_entries}"

    def _render_layout(self):
        """Ricrea il layout applicando palette e stato corrente."""
        self.palette = self._get_palette()
        self.page.bgcolor = self.palette["app_bg"]

        top_panel = self._create_top_panel()
        main_panel = self._create_main_panel()

        footer = ft.Container(
            content=ft.Row([
                ft.Text(
                    f"Lingue caricate: {', '.join(self.manager.languages)}",
                    size=11,
                    color=self.palette["text_secondary"],
                ),
                ft.Text(
                    f"Scope caricate: {len(self.manager.scopes)}",
                    size=11,
                    color=self.palette["text_secondary"],
                ),
            ], spacing=20),
            padding=10,
            bgcolor=self.palette["panel_bg"],
            border=ft.border.all(1, self.palette["border"]),
        )

        self.page.controls.clear()
        self.page.add(
            ft.Column(
                controls=[
                    top_panel,
                    ft.Divider(height=1),
                    main_panel,
                    footer,
                ],
                spacing=0,
                expand=True,
            )
        )
        self._update_scope_entries_info()
        self.page.update()
    
    def _init_ui(self):
        """Inizializza l'interfaccia utente."""
        # Carica i file i18n
        try:
            if not self.manager.load_all_files():
                self._show_error("Nessun file i18n trovato nella cartella /data")
                return
            
            if self.manager.scopes:
                self.current_scope = self.manager.scopes[0]
        except Exception as e:
            self._show_error(f"Errore nel caricamento: {e}")
            return

        self._render_layout()
    
    def _create_top_panel(self) -> ft.Container:
        """Crea il panel superiore con scope, ricerca e pulsanti."""
        
        # Dropdown per selezionare scope
        scope_options = [
            ft.dropdown.Option(str(s), self.manager.get_scope_label(s))
            for s in self.manager.scopes
        ]
        
        self.scope_dropdown = ft.Dropdown(
            label="Scope",
            options=scope_options,
            value=str(self.current_scope),
            width=150,
            on_select=self._on_scope_changed
        )
        
        # Campo ricerca
        self.search_field = ft.TextField(
            label="Ricerca testo",
            hint_text="Cerca in tutti gli scope e lingue...",
            width=300,
            on_submit=self._on_search
        )
        
        # Pulsanti ricerca
        search_btn = ft.IconButton(
            ft.icons.SEARCH,
            tooltip="Cerca (Enter)",
            on_click=self._on_search
        )
        
        self.prev_match_btn = ft.IconButton(
            ft.icons.ARROW_BACK,
            tooltip="Occorrenza precedente",
            on_click=self._on_prev_match,
            disabled=True
        )
        
        self.next_match_btn = ft.IconButton(
            ft.icons.ARROW_FORWARD,
            tooltip="Occorrenza successiva",
            on_click=self._on_next_match,
            disabled=True
        )
        
        self.search_status = ft.Text("", size=11, color=ft.colors.GREY_700)
        self.scope_entries_info = ft.Text("", size=11, color=self.palette["text_secondary"])
        
        # Pulsanti azioni
        save_btn = ft.ElevatedButton(
            "💾 SALVA",
            on_click=self._on_save,
            bgcolor=self.palette["success"],
            color="#FFFFFF",
        )
        
        exit_btn = ft.ElevatedButton(
            "🚪 ESCI",
            on_click=self._on_exit,
            bgcolor=self.palette["danger"],
            color="#FFFFFF",
        )
        
        # Checkbox traduzione automatica
        self.auto_translate_check = ft.Checkbox(
            label="Traduzione automatica",
            value=self.translator_config.is_enabled(),
            on_change=self._on_auto_translate_changed
        )

        # Switch tema chiaro/scuro
        self.theme_switch = ft.Switch(
            label="Tema scuro",
            value=self.page.theme_mode == ft.ThemeMode.DARK,
            on_change=self._on_theme_changed
        )
        
        # Layout
        return ft.Container(
            content=ft.Column([
                ft.Row([
                    ft.Text("Configurazione:", size=12, weight="bold", color=self.palette["text_primary"]),
                    self.scope_dropdown,
                    self.auto_translate_check,
                    self.theme_switch,
                    self.scope_entries_info,
                ], spacing=20, wrap=True),
                
                ft.Row([
                    ft.Text("Ricerca:", size=12, weight="bold", color=self.palette["text_primary"]),
                    self.search_field,
                    search_btn,
                    self.prev_match_btn,
                    self.next_match_btn,
                    self.search_status,
                    ft.Container(expand=True),
                    save_btn,
                    exit_btn,
                ], spacing=10, wrap=True, alignment=ft.MainAxisAlignment.START),
            ], spacing=10),
            padding=15,
            bgcolor=self.palette["panel_bg"],
            border=ft.border.all(1, self.palette["border"]),
        )
    
    def _create_main_panel(self) -> ft.Column:
        """Crea il panel principale con la tabella di edit."""

        self.rows_info_text = ft.Text(
            "",
            size=11,
            color=self.palette["text_secondary"],
        )

        info_row = ft.Container(
            content=self.rows_info_text,
            padding=8,
            bgcolor=self.palette["panel_bg"],
            border=ft.border.all(1, self.palette["border"]),
        )

        columns = [
            ft.DataColumn(ft.Text("ID", color=self.palette["table_header_text"])),
            ft.DataColumn(ft.Text("Nome ID", color=self.palette["table_header_text"])),
        ]

        for lang in self.manager.languages:
            lang_name = self.manager.get_language_name(lang)
            columns.append(
                ft.DataColumn(
                    ft.Text(f"{lang_name} ({lang})", color=self.palette["table_header_text"])
                )
            )

        self.data_table = ft.DataTable(
            columns=columns,
            rows=[],
            heading_row_color=self.palette["table_header_bg"],
            heading_text_style=ft.TextStyle(
                color=self.palette["table_header_text"],
                weight=ft.FontWeight.BOLD,
                size=12,
            ),
            data_text_style=ft.TextStyle(color=self.palette["text_primary"], size=11),
            divider_thickness=1,
            horizontal_lines=ft.border.BorderSide(1, self.palette["border"]),
            vertical_lines=ft.border.BorderSide(1, self.palette["border"]),
            data_row_min_height=56,
            data_row_max_height=180,
            column_spacing=16,
            border=ft.border.all(1, self.palette["border"]),
            bgcolor=self.palette["panel_bg"],
        )

        table_scroll = ft.ListView(
            controls=[
                ft.Row([self.data_table], scroll=ft.ScrollMode.AUTO),
            ],
            spacing=0,
            padding=0,
            expand=True,
        )

        # Popola le righe iniziali
        self._populate_data_rows(refresh_page=False)

        return ft.Container(
            content=ft.Column(
                controls=[
                    info_row,
                    ft.Divider(height=1),
                    table_scroll,
                ],
                spacing=0,
                expand=True,
            ),
            expand=True,
            bgcolor=self.palette["panel_bg"],
            border=ft.border.all(1, self.palette["border"]),
        )
    
    def _populate_data_rows(self, refresh_page: bool = True):
        """Popola le righe dati nella tabella."""
        self.edit_fields.clear()

        if not self.current_scope:
            if hasattr(self, 'rows_info_text'):
                self.rows_info_text.value = "Nessuno scope selezionato"
            if hasattr(self, 'data_table'):
                self.data_table.rows = []
            if refresh_page:
                self.page.update()
            return

        entries = self.manager.get_scope_data(self.current_scope)
        self.rows_info_text.value = f"Righe caricate: {len(entries)}"

        table_rows = []

        if not entries:
            empty_cells = [
                ft.DataCell(ft.Text("-", color=self.palette["text_secondary"])),
                ft.DataCell(ft.Text("Nessun dato disponibile per questo scope", color=self.palette["text_secondary"])),
            ]
            for _ in self.manager.languages:
                empty_cells.append(ft.DataCell(ft.Text("", color=self.palette["text_secondary"])))
            table_rows.append(ft.DataRow(cells=empty_cells))
        else:
            for row_index, entry in enumerate(entries):
                key = entry['key']
                section = entry.get('section', 0)
                translations = entry.get('translations', {})
                key_name = self.manager.get_key_name(key)
                row_bg = self.palette["row_bg_even"] if row_index % 2 == 0 else self.palette["row_bg_odd"]

                name_label = key_name if key_name else "-"
                if section and int(section) > 0:
                    name_label = f"{name_label} [s{section}]"

                cells = [
                    ft.DataCell(ft.Text(str(key), selectable=True, color=self.palette["text_primary"])),
                    ft.DataCell(ft.Text(name_label, selectable=True, color=self.palette["text_primary"])),
                ]

                for lang in self.manager.languages:
                    current_text = translations.get(lang, '')

                    text_field = ft.TextField(
                        value=current_text,
                        multiline=True,
                        min_lines=1,
                        max_lines=3,
                        width=230,
                        on_change=lambda e, s=self.current_scope, k=key, sec=section, l=lang:
                            self._on_text_changed(s, k, sec, l, e),
                        border_color=self.palette["border"] if lang != 'it' else self.palette["accent"],
                        border_width=2 if lang == 'it' else 1,
                        color=self.palette["text_primary"],
                        filled=True,
                        fill_color=self.palette["input_bg"],
                    )

                    self.edit_fields[(self.current_scope, key, section, lang)] = text_field

                    if lang != 'it':
                        translate_btn = ft.IconButton(
                            ft.icons.TRANSLATE,
                            tooltip=f"Traduci da Italiano a {lang}",
                            on_click=lambda e, s=self.current_scope, k=key, sec=section, l=lang:
                                self._on_translate(s, k, sec, l, e),
                            disabled=not self.translator_config.is_enabled(),
                            icon_color=self.palette["accent"],
                        )
                        cell_control = ft.Row([text_field, translate_btn], spacing=4, tight=True)
                    else:
                        cell_control = text_field

                    cells.append(ft.DataCell(cell_control))

                table_rows.append(
                    ft.DataRow(
                        cells=cells,
                        color={ft.ControlState.DEFAULT: row_bg},
                    )
                )

        self.data_table.rows = table_rows
        self._update_scope_entries_info()
        if refresh_page:
            self.page.update()
    
    def _on_scope_changed(self, e):
        """Cambio dello scope selezionato."""
        try:
            self.current_scope = int(e.control.value)
            self._populate_data_rows(refresh_page=True)
        except Exception as err:
            self._show_error(f"Errore nel cambio scope: {err}")
    
    def _on_text_changed(self, scope: int, key: int, section: int, lang: str, e):
        """Cambio di testo in un campo edit."""
        new_text = e.control.value
        
        # Aggiorna i dati in memoria
        self.manager.update_translation(scope, key, section, lang, new_text)
        
        self.has_changes = True
        self._update_title()
    
    def _on_search(self, e=None):
        """Ricerca di testo."""
        search_text = self.search_field.value.strip()
        
        if not search_text:
            self._show_error("Inserisci un testo da cercare")
            return
        
        self.search_results = self.manager.search_all_scopes(search_text)
        
        if not self.search_results:
            self._show_error(f"Nessun risultato per: {search_text}")
            self.search_results = []
            return
        
        self.search_index = 0
        self.prev_match_btn.disabled = False
        self.next_match_btn.disabled = False
        
        self._show_search_result(0)
    
    def _show_search_result(self, index: int):
        """Mostra il risultato della ricerca all'indice specificato."""
        if not self.search_results or index < 0 or index >= len(self.search_results):
            return
        
        scope, key, section, lang, text = self.search_results[index]
        
        # Cambia scope
        self.scope_dropdown.value = str(scope)
        self.current_scope = scope
        self._populate_data_rows()
        
        # Highlight il campo
        field_key = (scope, key, section, lang)
        if field_key in self.edit_fields:
            field = self.edit_fields[field_key]
            field.border_color = self.palette["highlight"]
            field.border_width = 3
        
        # Aggiorna status
        self.search_status.value = f"Risultato {index + 1}/{len(self.search_results)}"
        self.search_index = index
        self.page.update()
    
    def _on_prev_match(self, e):
        """Va al risultato di ricerca precedente."""
        if self.search_results:
            index = (self.search_index - 1) % len(self.search_results)
            self._show_search_result(index)
    
    def _on_next_match(self, e):
        """Va al risultato di ricerca successivo."""
        if self.search_results:
            index = (self.search_index + 1) % len(self.search_results)
            self._show_search_result(index)
    
    def _on_auto_translate_changed(self, e):
        """Abilita/disabilita la traduzione automatica."""
        self.translator_config.set_enabled(e.control.value)

        # Ricrea la tabella per aggiornare lo stato dei bottoni di traduzione
        self._populate_data_rows(refresh_page=True)

    def _on_theme_changed(self, e):
        """Cambia tema applicazione tra chiaro e scuro."""
        is_dark = bool(e.control.value)
        self.page.theme_mode = ft.ThemeMode.DARK if is_dark else ft.ThemeMode.LIGHT
        self._render_layout()
    
    def _on_translate(self, scope: int, key: int, section: int, lang: str, e):
        """Traduce automaticamente da italiano a un'altra lingua."""
        # TODO: Implementare la chiamata al servizio di traduzione
        self._show_error("Traduzione automatica: da implementare con API esterna")
    
    def _on_save(self, e):
        """Salva tutti i dati."""
        try:
            success, backup_file = self.manager.save_all_files()
            
            if success:
                self.has_changes = False
                self._update_title()
                self._show_success(f"✅ Dati salvati con successo!\nBackup: {backup_file}")
            else:
                self._show_error(f"❌ Errore nel salvataggio: {backup_file}")
        except Exception as err:
            self._show_error(f"❌ Errore: {err}")
    
    def _on_exit(self, e):
        """Esce dall'applicazione."""
        if self.has_changes:
            dlg = ft.AlertDialog(
                title=ft.Text("Non salvato"),
                content=ft.Text("Hai delle modifiche non salvate. Vuoi uscire comunque?"),
                actions=[
                    ft.TextButton("Annulla", on_click=lambda e: self._close_dialog(dlg)),
                    ft.TextButton("Esci", on_click=lambda e: self.page.window_destroy()),
                ],
            )
            self.page.dialog = dlg
            dlg.open = True
            self.page.update()
        else:
            self.page.window_destroy()
    
    def _close_dialog(self, dlg):
        """Chiude un dialog."""
        dlg.open = False
        self.page.update()
    
    def _show_error(self, message: str):
        """Mostra un messaggio di errore."""
        dlg = ft.AlertDialog(
            title=ft.Text("Errore"),
            content=ft.Text(message),
            actions=[ft.TextButton("OK", on_click=lambda e: self._close_dialog(dlg))],
        )
        self.page.dialog = dlg
        dlg.open = True
        self.page.update()
    
    def _show_success(self, message: str):
        """Mostra un messaggio di successo."""
        dlg = ft.AlertDialog(
            title=ft.Text("Successo"),
            content=ft.Text(message),
            actions=[ft.TextButton("OK", on_click=lambda e: self._close_dialog(dlg))],
        )
        self.page.dialog = dlg
        dlg.open = True
        self.page.update()
    
    def _update_title(self):
        """Aggiorna il titolo della finestra con indicazione di modifiche."""
        marker = "⚫ " if self.has_changes else ""
        self.page.title = f"{marker}i18n Editor"
        self.page.update()


def main(page: ft.Page):
    """Punto di ingresso dell'applicazione."""
    app = I18nEditorApp(page)


if __name__ == "__main__":
    view_mode = os.getenv("I18N_EDITOR_VIEW", "").strip().lower()
    app_view = ft.AppView.WEB_BROWSER if view_mode in ("web", "browser") else ft.AppView.FLET_APP
    ft.run(main, view=app_view)
