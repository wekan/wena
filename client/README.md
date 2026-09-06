# Client

Native UI code follows Meteor WeKan's client boundary: everything that draws or
handles local input belongs here. `components/` mirrors Jade component areas; each
component will expose C89 functions that render with a caller-owned Nuklear context.
`features/` will compose those components into screens, replacing Meteor's feature
imports without mixing persistence or remote API code into the UI.
