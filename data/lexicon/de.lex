# Vox German announcement lexicon (de). UTF-8, "key = value" per line.
# '#' comments and blank lines are ignored; later keys override earlier ones.
# Edit this file to refine wording — no C++ changes needed. The table is embedded
# at build time (#34), so a rebuild picks up edits; loading it at runtime without
# a rebuild is the app's job (#39).

# --- Control roles -----------------------------------------------------------
role.button      = Schaltfläche
role.checkbox    = Kontrollkästchen
role.radiobutton = Optionsfeld
role.edit        = Eingabefeld
role.combobox    = Kombinationsfeld
role.listitem    = Listenelement
role.menuitem    = Menüpunkt
role.link        = Link
role.statictext  = Text
# role.unknown intentionally omitted — an unknown role is announced as nothing.

# --- Control states ----------------------------------------------------------
state.checked    = aktiviert
state.unchecked  = nicht aktiviert
state.mixed      = teilweise aktiviert
state.expanded   = erweitert
state.collapsed  = reduziert
state.selected   = ausgewählt
state.disabled   = nicht verfügbar
state.readonly   = schreibgeschützt
state.emptyvalue = leer
