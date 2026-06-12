# Vox English announcement lexicon (en). UTF-8, "key = value" per line.
# '#' comments and blank lines are ignored; later keys override earlier ones.
# Loaded when VOX_LANGUAGE=en (see de.lex for the loading rules, #61). This file
# is also the template for contributing further languages: copy it to
# "<tag>.lex", translate the values, and set "language" accordingly.
language = en

# --- Control roles -----------------------------------------------------------
role.button      = button
role.checkbox    = checkbox
role.radiobutton = radio button
role.edit        = edit
role.combobox    = combo box
role.listitem    = list item
role.menuitem    = menu item
role.link        = link
role.statictext  = text
# role.unknown intentionally omitted — an unknown role is announced as nothing.

# --- Control states ----------------------------------------------------------
state.checked    = checked
state.unchecked  = not checked
state.mixed      = partially checked
state.expanded   = expanded
state.collapsed  = collapsed
state.selected   = selected
state.disabled   = unavailable
state.readonly   = read-only
state.emptyvalue = blank
