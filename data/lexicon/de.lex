# Vox German announcement lexicon (de). UTF-8, "key = value" per line.
# '#' comments and blank lines are ignored; later keys override earlier ones.
# Edit this file to refine wording — no C++ changes needed. The app loads
# "lexicon\<tag>.lex" next to the executable at startup (tag from VOX_LANGUAGE,
# default "de"; VOX_LEXICON points at an explicit file instead), so edits apply
# on the next start (#61). This canonical table is also embedded at build time
# (#34) as the fallback whenever no usable file is found.
#
# Every lexicon file declares the language it stands for; a file is accepted
# only if the declaration matches the language it was loaded as and no required
# key is missing. To add a language, copy this file to "<tag>.lex", translate
# the values, and set "language" accordingly (see en.lex for the English one).
language = de

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
