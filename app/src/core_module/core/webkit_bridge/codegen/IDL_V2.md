# WebExtension IDL v2

IDL v2 describes the JavaScript API surface and its native C++ boundary. It is
case-sensitive and WebIDL-like.

## Lexical rules

- Identifiers match `[A-Za-z_][A-Za-z0-9_]*`.
- `//` and `/* ... */` comments are supported.
- Declarations and members end with `;`.
- Attributes precede the declaration or member they affect.

## Grammar

```ebnf
file          = { declaration } ;
declaration   = external-type | enum | dictionary | callback | interface ;

external-type = attributes, "external", "type", qualified-name, ";" ;
enum          = attributes, "enum", identifier, "{",
                enum-value, { enum-value }, "}", ";" ;
enum-value    = attributes, identifier, "=", string-literal, ";" ;

dictionary    = attributes, "dictionary", identifier, "{",
                { attributes, [ "optional" ], type, identifier,
                  [ "=", default-value ], ";" },
                "}", ";" ;

callback      = attributes, "callback", identifier, "=", return-type,
                "(", [ parameters ], ")", ";" ;

interface     = attributes, "interface", identifier, "{",
                { function | property | event }, "}", ";" ;

function      = attributes, "function", return-type, identifier,
                "(", [ parameters ], ")", ";" ;
property      = attributes, [ "readonly" ], "property", type,
                identifier, ";" ;
event         = attributes, "event", identifier,
                ( "(", [ parameters ], ")" | ":", qualified-name ),
                [ "with", "(", [ parameters ], ")" ], ";" ;

parameters    = parameter, { ",", parameter } ;
parameter     = attributes, [ "optional" ], type, identifier,
                [ "=", default-value ] ;

return-type   = "void" | type ;
type          = primary, [ "?" ]
              | "(", type, "|", type, { "|", type }, ")" ;
primary       = "any" | "boolean" | "integer" | "number" | "string"
              | "object" | "undefined" | qualified-name
              | "sequence", "<", type, ">"
              | "Promise", "<", (type | "void"), ">" ;

default-value = string-literal | number-literal | "true" | "false"
              | "null" | "undefined" | "[]" | "{}" ;

attributes    = { "[", attribute, { ",", attribute }, "]" } ;
attribute     = identifier, [ "=", attribute-value ] ;
qualified-name = identifier, { ".", identifier } ;
```

`[Tags=...]` is a dedicated comma-separated tag list, for example
`[Tags=Chrome,BackgroundScript]`.

## Types and absence

- `optional T` means the argument or dictionary member may be absent.
- `any` accepts every supplied JavaScript value, including `null` and
  `undefined`.
- `sequence<T>` accepts arrays whose elements match `T`.
- `Promise<(T | undefined)>` maps to native `Promise<T>`; native code resolves
  the undefined case with `resolve()` and no value.
- Dictionary members are required unless marked `optional`.
- `[AdditionalProperties=false]` rejects undeclared dictionary properties.

An optional argument or dictionary member may declare a default with `=`. The
default is applied only when the value is absent; an explicitly supplied
`null` or `undefined` remains a value and must match the declared type. Defaults
must be representable by the declared type and are materialized before native
dispatch, so native implementations receive the effective value.

## Attributes

| Attribute | Applies to | Meaning |
| --- | --- | --- |
| `Tags=...` | declarations, members, enum values, arguments | Root and execution-context visibility. |
| `Permissions="name|name"` | interface | Manifest permissions required to install the API facade and use its native boundary. All listed permissions are required. |
| `Global="path"` | interface | JavaScript installation path below `chrome` or `browser`. |
| `Id="value"` | function, event | Stable declaration identifier. |
| `Implementation=Native("target")` | interface, function, property, event | Dispatch to a generated native target. |
| `Implementation=JavaScript("target")` | interface, function, property, event | Dispatch to registered JavaScript after overload resolution. |
| `Implementation=Redirect("target")` | function, property | Forward to another compatible generated target. |
| `Internal` | function, event | Omit from the public facade; expose functions through `invokeNative` and events through `internal`. |
| `AdditionalProperties=false` | dictionary | Use a closed structural dictionary. |
| `Deprecated` | declaration or member | Preserve the API while marking it deprecated. |
| `Min`, `Max`, `MinLength`, `MaxLength`, `Pattern` | value or argument | Apply schema validation constraints. |

`Implementation=Native` with the public function name as its target is the
default. An interface-level implementation is inherited by members without an
override. JavaScript implementations receive `{id, args, internal, invokeNative}`
after generated overload matching and default application. `internal` exposes
only members declared with `[Internal]`; it is not installed on the public API
object. If `Id` is omitted, it is
generated from the function name, return type, and argument types, including
`optional` markers. Generated IDs must be unique within an interface.

## Visibility

Root tags are `Chrome` and `Firefox`; omitting them means both. Execution tags
include `BackgroundScript`, `ContentScript`, `ExtensionPage`, `DevToolsPage`,
`OffscreenDocument`, and `UserScript`. `Everywhere` means all execution
contexts.

Generation can be filtered with alternatives separated by `|`:

```text
generate_v2.go --visibility="BackgroundScript|ContentScript"
```

The installed extension schema does not select an API root. Both retained
`chrome` and `browser` projections are installed.

The generator emits the JavaScript facade as composable interface fragments.
At runtime the browser selects fragments using the native execution profile
and the extension's active permissions. The same generated permission metadata
is checked again by native call and event dispatch; facade filtering is not a
security boundary.

An interface without `Global` describes a returned object rather
than installing a global API namespace. `external type` references a host or
another API's type without generating its storage or implementation.

## Overloads

Resolution structurally matches supplied values, supports optional arguments
in any position. A call fails with `TypeError` when no overload matches or when
the best match is ambiguous.

Callback and Promise forms are separate declarations.

The parameter-list event form declares a `void` listener implicitly. The typed
form, `event name: CallbackType`, is used when listener identity or its return
type is significant, such as `runtime.onMessage`.

An event's optional `with (...)` clause declares the arguments following the
listener in `addListener(listener, ...)`. They use normal parameter matching,
validation, optionality, and defaults. The normalized registration object is
retained by the native listener; for example:

```webidl
event onCompleted: WebRequestOnCompletedListener with (
    WebRequestRequestFilter filter,
    optional sequence<WebRequestOnCompletedOption> extraInfoSpec
);
```

## C++ mapping

Generated C++ declarations and serialization helpers are placed in
`WebExtensions::v2`.

- Required arguments and dictionary members use `const T&` in view types.
- Optional arguments and members use `const T*` in view types.
- Owning `Storage` types use values and `std::optional<T>`.
- Generated `from_storage` and `to_storage` functions convert between view and storage.
- `Promise<T>` owns the extension ID and native `SecurityProfile`; native
  functions do not receive separate identity arguments. Listener registrations
  retain the same profile for event-delivery authorization.
