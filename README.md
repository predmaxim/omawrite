# Omawrite

A dead-simple Markdown writing app built with Qt Quick and C++ that automatically follows system dark/light mode.

<img width="2948" height="3227" alt="screenshot-2026-06-23_15-24-08" src="https://github.com/user-attachments/assets/4e930c0d-edda-4046-b444-a59eff523329" />
<img width="2948" height="3227" alt="screenshot-2026-06-23_15-23-23" src="https://github.com/user-attachments/assets/8ced7c26-961b-4ded-b263-84403001a951" />


## Install

Install via the Omarchy Package Repository via the `omawrite` package. It's installed by default in new installations of Omarchy (from Quattro forward).

## Shortcuts

- `Ctrl+S` saves. Unsaved documents use the XDG desktop portal file picker.
- `Ctrl+Shift+S` saves as.
- `Ctrl+O` opens a Markdown file through the portal picker.
- `Ctrl+P` opens the system print dialog.
- `Ctrl+N` opens a new Omawrite window.
- `Ctrl+Z`, `Ctrl+Shift+Z`, and `Ctrl+Y` handle undo and redo.
- `Super+F` toggles fullscreen. Qt maps this key as `Meta+F`.
- `Ctrl+F` searches the document. Use `Enter` or `Ctrl+G` for the next match and `Shift+Enter` for the previous match.
- `Ctrl+H` opens find and replace.
- `Ctrl+B`, `Ctrl+I`, and `Ctrl+K` insert bold, italic, and link Markdown.
- `Ctrl+Enter` toggles the task box on the caret's line.
- `Ctrl+=` (or `Ctrl++`), `Ctrl+-`, and `Ctrl+Wheel` zoom the editor text; `Ctrl+0`
  resets it. The zoom is remembered across runs.
- `Ctrl+?` shows the keyboard shortcut reference.

Unsaved drafts are recovered after an abnormal exit. Omawrite also watches open files
and warns before an external change can replace local work.

Text follows the desktop text size — `omarchy display text size`, or GNOME's
`text-scaling-factor` — and re-flows without a restart. The default of 12px leaves
Omawrite at the size it is designed around; larger and smaller sizes scale from there.

## Writing

The editor keeps the file as plain Markdown and renders it in place. The line with
the caret shows its raw syntax; everywhere else the syntax is hidden or drawn:

- Headings, bold, italic, strikethrough, inline code and links lose their markers.
- Fenced code blocks are colored by language (`js`, `ts`, `python`, `bash`, `go`,
  `rust`, `sql`, `json`, `yaml`, …). The ```` ``` ```` fences show only while the
  caret is inside the block.
- Task lists (`- [ ]`, `- [x]`) draw clickable boxes.
- `---`, `***` and `___` draw a horizontal rule.
- Tables draw as a grid: a bold header, column alignment from the delimiter row
  (`:--`, `--:`, `:-:`), and cells that wrap. Inline Markdown in cells is rendered.
  The whole table turns back into raw Markdown while the caret is in it.

The text column is at most 100 characters wide and follows the zoom.

## Requirements

- Qt 6: `qt6-base`, `qt6-declarative`, `qt6-quickcontrols2`
- `xdg-desktop-portal` and a portal backend

The iA Writer Mono font is bundled under the SIL Open Font License 1.1; see
`fonts/OFL.txt`. The font is copyright Information Architects Inc. and based on
IBM Plex, copyright IBM Corp.
