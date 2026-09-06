# WeKan - WeKan Native

- Made with C89, SDL2, Nuklear GUI, SQLite.
- Drag drop, looks same like Meteor 3 WeKan.
- For all desktop and mobile operating systems.
- Based on Meteor 3 WeKan https://github.com/wekan/wekan/models
- Local mode: Uses local SQLite database for read and write
- Remote mode: Uses WeKan REST API for read and write
- Uses WeKan Jade UI layout, with Nuclear UI components

# Roadmap

- [_] Add GitHub Actions release-all.yml that crosscompiles for many operating systems
- Operating systems at the beginning at release-all.yml:
  - [_] Linux arm64
  - [_] Linux amd64
  - [_] Linux armhf
  - [_] Windows amd64
  - [_] macOS arm64
  - [_] AmigaOS 3.x m68k
  - [_] AROS x86
- [_] Using same directory structure like Meteor 3 WeKan, save files as C89 and Nuclear GUI code
- [_] Convert Meteor 3 schema to SQLite schema that is optimized for fast queries
- [_] Using Nuclear GUI components, create same UI layout
- [_] Import/Export from WeKan, Trello, etc via WeKan REST API, Trello API, etc
- [_] Nuclear GUI adapts to all screen sizes from smallest to biggest, with mobile and desktop mode, like Meteor 3 WeKan
- [_] GUI works with touch displays, mouse, keyboard
- [_] Possible to drag drop same way like Meteor 3 WeKan
- [_] Collapse Swimlane, List, Card etc like Meteor 3 WeKan
