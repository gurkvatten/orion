package main

import (
	"flag"
	"fmt"
	"os"
	"regexp"
	"sort"
	"strconv"
	"strings"
	"unicode"
	"unicode/utf8"
)

// The v2 generator is intentionally separate from generate.go.  V1 IDLs keep
// their parser and ABI while v2 IDLs opt into the WebExtensions::v2 ABI.

type tokenKind int

const (
	tokEOF tokenKind = iota
	tokIdent
	tokString
	tokNumber
	tokSymbol
)

type token struct {
	kind tokenKind
	text string
	line int
	col  int
}

type lexer struct {
	src  []rune
	pos  int
	line int
	col  int
}

func newLexer(src string) *lexer { return &lexer{src: []rune(src), line: 1, col: 1} }

func (l *lexer) peek(offset int) rune {
	idx := l.pos + offset
	if idx >= len(l.src) {
		return 0
	}
	return l.src[idx]
}

func (l *lexer) advance() rune {
	r := l.peek(0)
	if r == 0 {
		return 0
	}
	l.pos++
	if r == '\n' {
		l.line++
		l.col = 1
	} else {
		l.col++
	}
	return r
}

func (l *lexer) skipWhitespaceAndComments() error {
	for {
		for unicode.IsSpace(l.peek(0)) {
			l.advance()
		}
		if l.peek(0) == '/' && l.peek(1) == '/' {
			for l.peek(0) != 0 && l.peek(0) != '\n' {
				l.advance()
			}
			continue
		}
		if l.peek(0) == '/' && l.peek(1) == '*' {
			startLine, startCol := l.line, l.col
			l.advance()
			l.advance()
			for !(l.peek(0) == '*' && l.peek(1) == '/') {
				if l.peek(0) == 0 {
					return fmt.Errorf("%d:%d: unterminated block comment", startLine, startCol)
				}
				l.advance()
			}
			l.advance()
			l.advance()
			continue
		}
		return nil
	}
}

func (l *lexer) next() (token, error) {
	if err := l.skipWhitespaceAndComments(); err != nil {
		return token{}, err
	}
	line, col := l.line, l.col
	r := l.peek(0)
	if r == 0 {
		return token{kind: tokEOF, line: line, col: col}, nil
	}
	if unicode.IsLetter(r) || r == '_' {
		var b strings.Builder
		for unicode.IsLetter(l.peek(0)) || unicode.IsDigit(l.peek(0)) || l.peek(0) == '_' {
			b.WriteRune(l.advance())
		}
		return token{kind: tokIdent, text: b.String(), line: line, col: col}, nil
	}
	if unicode.IsDigit(r) || (r == '-' && unicode.IsDigit(l.peek(1))) {
		var b strings.Builder
		b.WriteRune(l.advance())
		for unicode.IsDigit(l.peek(0)) {
			b.WriteRune(l.advance())
		}
		return token{kind: tokNumber, text: b.String(), line: line, col: col}, nil
	}
	if r == '"' {
		var b strings.Builder
		b.WriteRune(l.advance())
		for {
			r = l.peek(0)
			if r == 0 || r == '\n' {
				return token{}, fmt.Errorf("%d:%d: unterminated string", line, col)
			}
			b.WriteRune(l.advance())
			if r == '\\' {
				if l.peek(0) == 0 {
					return token{}, fmt.Errorf("%d:%d: unterminated string escape", line, col)
				}
				b.WriteRune(l.advance())
				continue
			}
			if r == '"' {
				break
			}
		}
		value, err := strconv.Unquote(b.String())
		if err != nil {
			return token{}, fmt.Errorf("%d:%d: invalid string: %w", line, col, err)
		}
		return token{kind: tokString, text: value, line: line, col: col}, nil
	}
	if r == '.' && l.peek(1) == '.' && l.peek(2) == '.' {
		l.advance()
		l.advance()
		l.advance()
		return token{kind: tokSymbol, text: "...", line: line, col: col}, nil
	}
	if strings.ContainsRune("[](){}<>,;=?.|:", r) {
		l.advance()
		return token{kind: tokSymbol, text: string(r), line: line, col: col}, nil
	}
	return token{}, fmt.Errorf("%d:%d: unexpected character %q", line, col, r)
}

type typeRef struct {
	Name     string
	Elem     *typeRef
	Arms     []typeRef
	Nullable bool
}

func (t typeRef) key() string {
	result := t.Name
	if t.Name == "union" {
		arms := make([]string, 0, len(t.Arms))
		for _, arm := range t.Arms {
			arms = append(arms, arm.key())
		}
		result = "(" + strings.Join(arms, "|") + ")"
	}
	if t.Elem != nil {
		result += "<" + t.Elem.key() + ">"
	}
	if t.Nullable {
		result += "?"
	}
	return result
}

type parameter struct {
	Name     string
	Type     typeRef
	Optional bool
	Attrs    attributes
	Default  defaultValue
}

type fieldDef struct {
	Name     string
	Type     typeRef
	Optional bool
	Tags     []string
	Default  defaultValue
}

type dictionaryDef struct {
	Name                 string
	Fields               []fieldDef
	AdditionalProperties bool
	Tags                 []string
}

type callbackDef struct {
	Name       string
	Return     typeRef
	ReturnVoid bool
	Params     []parameter
	Tags       []string
}

type defaultValue struct {
	Set  bool
	Kind tokenKind
	Text string
}

func (v defaultValue) js() string {
	if !v.Set {
		return "undefined"
	}
	switch v.Kind {
	case tokString:
		return strconv.Quote(v.Text)
	case tokNumber:
		return v.Text
	default:
		return v.Text
	}
}

type externalTypeDef struct {
	Name string
	Tags []string
}

type enumValueDef struct {
	Name  string
	Value string
	Tags  []string
}

type enumDef struct {
	Name   string
	Values []enumValueDef
	Tags   []string
}

type implementationAttr struct {
	Kind   string
	Target string
}

type functionDef struct {
	Name        string
	Return      typeRef
	ReturnVoid  bool
	Params      []parameter
	Tags        []string
	Permissions []string
	ID          string
	Impl        implementationAttr
	Internal    bool
}

func defaultFunctionID(fn functionDef) string {
	returnType := "void"
	if !fn.ReturnVoid {
		returnType = fn.Return.key()
	}
	parameterTypes := make([]string, 0, len(fn.Params))
	for _, param := range fn.Params {
		parameterType := param.Type.key()
		if param.Optional {
			parameterType = "optional " + parameterType
		}
		parameterTypes = append(parameterTypes, parameterType)
	}
	return fn.Name + ":" + returnType + "(" + strings.Join(parameterTypes, ",") + ")"
}

type eventDef struct {
	Name               string
	Callback           string
	Params             []parameter
	RegistrationParams []parameter
	Tags               []string
	ID                 string
	Impl               implementationAttr
	Internal           bool
}

type propertyDef struct {
	Name     string
	Type     typeRef
	Readonly bool
	Tags     []string
	ID       string
	Impl     implementationAttr
	Internal bool
}

type interfaceDef struct {
	Name        string
	Global      string
	Tags        []string
	Permissions []string
	Impl        implementationAttr
	Functions   []functionDef
	Properties  []propertyDef
	Events      []eventDef
}

func parsePermissions(attrs attributes) []string {
	value, ok := attrs.Values["Permissions"]
	if !ok || strings.TrimSpace(value.Text) == "" {
		return nil
	}
	var result []string
	for _, permission := range strings.Split(value.Text, "|") {
		permission = strings.TrimSpace(permission)
		if permission != "" {
			result = append(result, permission)
		}
	}
	return result
}

type idlFile struct {
	ExternalTypes []externalTypeDef
	Enums         []enumDef
	Dictionaries  []dictionaryDef
	Callbacks     []callbackDef
	Interfaces    []interfaceDef
}

type attrValue struct {
	Name string
	Text string
	Call string
}

type attributes struct {
	Tags   []string
	Values map[string]attrValue
}

type parser struct {
	path   string
	lex    *lexer
	peeked *token
}

func newParser(path, src string) *parser { return &parser{path: path, lex: newLexer(src)} }

func (p *parser) next() (token, error) {
	if p.peeked != nil {
		t := *p.peeked
		p.peeked = nil
		return t, nil
	}
	return p.lex.next()
}

func (p *parser) peek() (token, error) {
	if p.peeked == nil {
		t, err := p.lex.next()
		if err != nil {
			return token{}, err
		}
		p.peeked = &t
	}
	return *p.peeked, nil
}

func (p *parser) errorf(t token, format string, args ...any) error {
	return fmt.Errorf("%s:%d:%d: %s", p.path, t.line, t.col, fmt.Sprintf(format, args...))
}

func (p *parser) accept(text string) (bool, error) {
	t, err := p.peek()
	if err != nil {
		return false, err
	}
	if t.text != text {
		return false, nil
	}
	_, err = p.next()
	return true, err
}

func (p *parser) expect(text string) (token, error) {
	t, err := p.next()
	if err != nil {
		return token{}, err
	}
	if t.text != text {
		return token{}, p.errorf(t, "expected %q, got %q", text, t.text)
	}
	return t, nil
}

func (p *parser) expectIdent() (string, error) {
	t, err := p.next()
	if err != nil {
		return "", err
	}
	if t.kind != tokIdent {
		return "", p.errorf(t, "expected identifier, got %q", t.text)
	}
	return t.text, nil
}

func (p *parser) parseQualifiedName() (string, error) {
	name, err := p.expectIdent()
	if err != nil {
		return "", err
	}
	for {
		dot, err := p.accept(".")
		if err != nil || !dot {
			return name, err
		}
		part, err := p.expectIdent()
		if err != nil {
			return "", err
		}
		name += "." + part
	}
}

func (p *parser) parseDefaultValue() (defaultValue, error) {
	t, err := p.next()
	if err != nil {
		return defaultValue{}, err
	}
	switch t.kind {
	case tokString, tokNumber:
		return defaultValue{Set: true, Kind: t.kind, Text: t.text}, nil
	case tokIdent:
		if t.text == "true" || t.text == "false" || t.text == "null" || t.text == "undefined" {
			return defaultValue{Set: true, Kind: t.kind, Text: t.text}, nil
		}
	case tokSymbol:
		if t.text == "[" {
			if _, err := p.expect("]"); err != nil {
				return defaultValue{}, err
			}
			return defaultValue{Set: true, Kind: t.kind, Text: "[]"}, nil
		}
		if t.text == "{" {
			if _, err := p.expect("}"); err != nil {
				return defaultValue{}, err
			}
			return defaultValue{Set: true, Kind: t.kind, Text: "{}"}, nil
		}
	}
	return defaultValue{}, p.errorf(t, "invalid default value %q", t.text)
}

func (p *parser) parseAttributes() (attributes, error) {
	result := attributes{Values: make(map[string]attrValue)}
	for {
		ok, err := p.accept("[")
		if err != nil || !ok {
			return result, err
		}
		name, err := p.expectIdent()
		if err != nil {
			return result, err
		}
		if name == "Tags" {
			if _, err := p.expect("="); err != nil {
				return result, err
			}
			for {
				tag, err := p.expectIdent()
				if err != nil {
					return result, err
				}
				result.Tags = append(result.Tags, tag)
				comma, err := p.accept(",")
				if err != nil {
					return result, err
				}
				if !comma {
					break
				}
			}
			if _, err := p.expect("]"); err != nil {
				return result, err
			}
			continue
		}

		for {
			value := attrValue{Name: name}
			hasValue, err := p.accept("=")
			if err != nil {
				return result, err
			}
			if hasValue {
				v, err := p.next()
				if err != nil {
					return result, err
				}
				if v.kind == tokString || v.kind == tokNumber {
					value.Text = v.text
				} else if v.kind == tokIdent {
					if call, err := p.accept("("); err != nil {
						return result, err
					} else if call {
						arg, err := p.next()
						if err != nil {
							return result, err
						}
						if arg.kind != tokString && arg.kind != tokIdent {
							return result, p.errorf(arg, "expected attribute call argument")
						}
						if _, err := p.expect(")"); err != nil {
							return result, err
						}
						value.Call = v.text
						value.Text = arg.text
					} else {
						value.Text = v.text
					}
				} else {
					return result, p.errorf(v, "invalid attribute value")
				}
			}
			result.Values[name] = value
			comma, err := p.accept(",")
			if err != nil {
				return result, err
			}
			if !comma {
				break
			}
			name, err = p.expectIdent()
			if err != nil {
				return result, err
			}
		}
		if _, err := p.expect("]"); err != nil {
			return result, err
		}
	}
}

func (p *parser) parseType() (typeRef, error) {
	if union, err := p.accept("("); err != nil {
		return typeRef{}, err
	} else if union {
		result := typeRef{Name: "union"}
		first, err := p.parseType()
		if err != nil {
			return typeRef{}, err
		}
		result.Arms = append(result.Arms, first)
		if _, err := p.expect("|"); err != nil {
			return typeRef{}, err
		}
		for {
			arm, err := p.parseType()
			if err != nil {
				return typeRef{}, err
			}
			result.Arms = append(result.Arms, arm)
			more, err := p.accept("|")
			if err != nil {
				return typeRef{}, err
			}
			if !more {
				break
			}
		}
		if _, err := p.expect(")"); err != nil {
			return typeRef{}, err
		}
		if nullable, err := p.accept("?"); err != nil {
			return typeRef{}, err
		} else {
			result.Nullable = nullable
		}
		return result, nil
	}
	name, err := p.parseQualifiedName()
	if err != nil {
		return typeRef{}, err
	}
	result := typeRef{Name: name}
	if name == "Promise" || name == "sequence" {
		if _, err := p.expect("<"); err != nil {
			return typeRef{}, err
		}
		if name == "Promise" {
			if t, err := p.peek(); err != nil {
				return typeRef{}, err
			} else if t.text == "void" {
				_, _ = p.next()
				elem := typeRef{Name: "void"}
				result.Elem = &elem
			} else {
				elem, err := p.parseType()
				if err != nil {
					return typeRef{}, err
				}
				result.Elem = &elem
			}
		} else {
			elem, err := p.parseType()
			if err != nil {
				return typeRef{}, err
			}
			result.Elem = &elem
		}
		if _, err := p.expect(">"); err != nil {
			return typeRef{}, err
		}
	}
	if nullable, err := p.accept("?"); err != nil {
		return typeRef{}, err
	} else {
		result.Nullable = nullable
	}
	return result, nil
}

func (p *parser) parseParams() ([]parameter, error) {
	var params []parameter
	if t, err := p.peek(); err != nil {
		return nil, err
	} else if t.text == ")" {
		return params, nil
	}
	for {
		attrs, err := p.parseAttributes()
		if err != nil {
			return nil, err
		}
		optional, err := p.accept("optional")
		if err != nil {
			return nil, err
		}
		typeName, err := p.parseType()
		if err != nil {
			return nil, err
		}
		if rest, err := p.accept("..."); err != nil {
			return nil, err
		} else if rest {
			return nil, fmt.Errorf("%s: rest parameters are not implemented by the initial v2 generator", p.path)
		}
		name, err := p.expectIdent()
		if err != nil {
			return nil, err
		}
		var defaultVal defaultValue
		if hasDefault, err := p.accept("="); err != nil {
			return nil, err
		} else if hasDefault {
			defaultVal, err = p.parseDefaultValue()
			if err != nil {
				return nil, err
			}
		}
		params = append(params, parameter{Name: name, Type: typeName, Optional: optional, Attrs: attrs, Default: defaultVal})
		comma, err := p.accept(",")
		if err != nil || !comma {
			return params, err
		}
	}
}

func (p *parser) parseDictionary(attrs attributes) (dictionaryDef, error) {
	name, err := p.expectIdent()
	if err != nil {
		return dictionaryDef{}, err
	}
	if _, err := p.expect("{"); err != nil {
		return dictionaryDef{}, err
	}
	result := dictionaryDef{Name: name, AdditionalProperties: true, Tags: attrs.Tags}
	if value, ok := attrs.Values["AdditionalProperties"]; ok && value.Text == "false" {
		result.AdditionalProperties = false
	}
	for {
		if done, err := p.accept("}"); err != nil {
			return dictionaryDef{}, err
		} else if done {
			break
		}
		memberAttrs, err := p.parseAttributes()
		if err != nil {
			return dictionaryDef{}, err
		}
		optional, err := p.accept("optional")
		if err != nil {
			return dictionaryDef{}, err
		}
		typeName, err := p.parseType()
		if err != nil {
			return dictionaryDef{}, err
		}
		fieldName, err := p.expectIdent()
		if err != nil {
			return dictionaryDef{}, err
		}
		var defaultVal defaultValue
		if hasDefault, err := p.accept("="); err != nil {
			return dictionaryDef{}, err
		} else if hasDefault {
			defaultVal, err = p.parseDefaultValue()
			if err != nil {
				return dictionaryDef{}, err
			}
		}
		if _, err := p.expect(";"); err != nil {
			return dictionaryDef{}, err
		}
		result.Fields = append(result.Fields, fieldDef{Name: fieldName, Type: typeName, Optional: optional, Tags: memberAttrs.Tags, Default: defaultVal})
	}
	_, err = p.expect(";")
	return result, err
}

func (p *parser) parseCallback(attrs attributes) (callbackDef, error) {
	name, err := p.expectIdent()
	if err != nil {
		return callbackDef{}, err
	}
	if _, err := p.expect("="); err != nil {
		return callbackDef{}, err
	}
	result := callbackDef{Name: name, Tags: attrs.Tags}
	if isVoid, err := p.accept("void"); err != nil {
		return callbackDef{}, err
	} else if isVoid {
		result.ReturnVoid = true
	} else {
		result.Return, err = p.parseType()
		if err != nil {
			return callbackDef{}, err
		}
	}
	if _, err := p.expect("("); err != nil {
		return callbackDef{}, err
	}
	result.Params, err = p.parseParams()
	if err != nil {
		return callbackDef{}, err
	}
	if _, err := p.expect(")"); err != nil {
		return callbackDef{}, err
	}
	_, err = p.expect(";")
	return result, err
}

func parseImpl(attrs attributes, defaultTarget string) implementationAttr {
	value, ok := attrs.Values["Implementation"]
	if !ok {
		return implementationAttr{Kind: "Native", Target: defaultTarget}
	}
	kind := value.Call
	if kind == "" {
		kind = value.Text
	}
	return implementationAttr{Kind: kind, Target: value.Text}
}

func (p *parser) parseExternalType(attrs attributes) (externalTypeDef, error) {
	if _, err := p.expect("type"); err != nil {
		return externalTypeDef{}, err
	}
	name, err := p.parseQualifiedName()
	if err != nil {
		return externalTypeDef{}, err
	}
	if _, err := p.expect(";"); err != nil {
		return externalTypeDef{}, err
	}
	return externalTypeDef{Name: name, Tags: attrs.Tags}, nil
}

func (p *parser) parseEnum(attrs attributes) (enumDef, error) {
	name, err := p.expectIdent()
	if err != nil {
		return enumDef{}, err
	}
	if _, err := p.expect("{"); err != nil {
		return enumDef{}, err
	}
	result := enumDef{Name: name, Tags: attrs.Tags}
	for {
		if done, err := p.accept("}"); err != nil {
			return enumDef{}, err
		} else if done {
			break
		}
		valueAttrs, err := p.parseAttributes()
		if err != nil {
			return enumDef{}, err
		}
		valueName, err := p.expectIdent()
		if err != nil {
			return enumDef{}, err
		}
		if _, err := p.expect("="); err != nil {
			return enumDef{}, err
		}
		value, err := p.next()
		if err != nil {
			return enumDef{}, err
		}
		if value.kind != tokString {
			return enumDef{}, p.errorf(value, "enum value must be a string")
		}
		if _, err := p.expect(";"); err != nil {
			return enumDef{}, err
		}
		result.Values = append(result.Values, enumValueDef{Name: valueName, Value: value.text, Tags: valueAttrs.Tags})
	}
	_, err = p.expect(";")
	return result, err
}

func (p *parser) parseInterface(attrs attributes) (interfaceDef, error) {
	name, err := p.expectIdent()
	if err != nil {
		return interfaceDef{}, err
	}
	result := interfaceDef{
		Name:        name,
		Tags:        attrs.Tags,
		Permissions: parsePermissions(attrs),
	}
	if _, ok := attrs.Values["Implementation"]; ok {
		result.Impl = parseImpl(attrs, name)
	}
	if global, ok := attrs.Values["Global"]; ok {
		result.Global = global.Text
	}
	if _, err := p.expect("{"); err != nil {
		return interfaceDef{}, err
	}
	for {
		if done, err := p.accept("}"); err != nil {
			return interfaceDef{}, err
		} else if done {
			break
		}
		memberAttrs, err := p.parseAttributes()
		if err != nil {
			return interfaceDef{}, err
		}
		kind, err := p.expectIdent()
		if err != nil {
			return interfaceDef{}, err
		}
		readonly := false
		if kind == "readonly" {
			readonly = true
			kind, err = p.expectIdent()
			if err != nil {
				return interfaceDef{}, err
			}
		}
		switch kind {
		case "function":
			_, internal := memberAttrs.Values["Internal"]
			fn := functionDef{
				Tags:        memberAttrs.Tags,
				Permissions: parsePermissions(memberAttrs),
				Internal:    internal,
			}
			if isVoid, err := p.accept("void"); err != nil {
				return interfaceDef{}, err
			} else if isVoid {
				fn.ReturnVoid = true
			} else {
				fn.Return, err = p.parseType()
				if err != nil {
					return interfaceDef{}, err
				}
			}
			fn.Name, err = p.expectIdent()
			if err != nil {
				return interfaceDef{}, err
			}
			if _, err := p.expect("("); err != nil {
				return interfaceDef{}, err
			}
			fn.Params, err = p.parseParams()
			if err != nil {
				return interfaceDef{}, err
			}
			if _, err := p.expect(")"); err != nil {
				return interfaceDef{}, err
			}
			if _, err := p.expect(";"); err != nil {
				return interfaceDef{}, err
			}
			if id, ok := memberAttrs.Values["Id"]; ok {
				fn.ID = id.Text
			}
			if fn.ID == "" {
				fn.ID = defaultFunctionID(fn)
			}
			if _, ok := memberAttrs.Values["Implementation"]; ok {
				fn.Impl = parseImpl(memberAttrs, fn.Name)
			} else if result.Impl.Kind != "" {
				fn.Impl = result.Impl
			} else {
				fn.Impl = parseImpl(memberAttrs, fn.Name)
			}
			result.Functions = append(result.Functions, fn)
		case "property":
			_, internal := memberAttrs.Values["Internal"]
			property := propertyDef{Readonly: readonly, Tags: memberAttrs.Tags, Internal: internal}
			property.Type, err = p.parseType()
			if err != nil {
				return interfaceDef{}, err
			}
			property.Name, err = p.expectIdent()
			if err != nil {
				return interfaceDef{}, err
			}
			if _, err := p.expect(";"); err != nil {
				return interfaceDef{}, err
			}
			if id, ok := memberAttrs.Values["Id"]; ok {
				property.ID = id.Text
			}
			if property.ID == "" {
				property.ID = property.Name
			}
			if _, ok := memberAttrs.Values["Implementation"]; ok {
				property.Impl = parseImpl(memberAttrs, property.Name)
			} else if result.Impl.Kind != "" {
				property.Impl = result.Impl
			} else {
				property.Impl = parseImpl(memberAttrs, property.Name)
			}
			result.Properties = append(result.Properties, property)
		case "event":
			_, internal := memberAttrs.Values["Internal"]
			event := eventDef{Tags: memberAttrs.Tags, Internal: internal}
			event.Name, err = p.expectIdent()
			if err != nil {
				return interfaceDef{}, err
			}
			if typed, err := p.accept(":"); err != nil {
				return interfaceDef{}, err
			} else if typed {
				event.Callback, err = p.parseQualifiedName()
				if err != nil {
					return interfaceDef{}, err
				}
			} else {
				if _, err := p.expect("("); err != nil {
					return interfaceDef{}, err
				}
				event.Params, err = p.parseParams()
				if err != nil {
					return interfaceDef{}, err
				}
				if _, err := p.expect(")"); err != nil {
					return interfaceDef{}, err
				}
			}
			if hasRegistrationParams, err := p.accept("with"); err != nil {
				return interfaceDef{}, err
			} else if hasRegistrationParams {
				if _, err := p.expect("("); err != nil {
					return interfaceDef{}, err
				}
				event.RegistrationParams, err = p.parseParams()
				if err != nil {
					return interfaceDef{}, err
				}
				if _, err := p.expect(")"); err != nil {
					return interfaceDef{}, err
				}
			}
			if _, err := p.expect(";"); err != nil {
				return interfaceDef{}, err
			}
			if id, ok := memberAttrs.Values["Id"]; ok {
				event.ID = id.Text
			}
			if event.ID == "" {
				event.ID = event.Name
			}
			if _, ok := memberAttrs.Values["Implementation"]; ok {
				event.Impl = parseImpl(memberAttrs, event.Name)
			} else if result.Impl.Kind != "" {
				event.Impl = result.Impl
			} else {
				event.Impl = implementationAttr{Kind: "Native", Target: event.Name}
			}
			result.Events = append(result.Events, event)
		default:
			return interfaceDef{}, fmt.Errorf("%s: unsupported interface member %q", p.path, kind)
		}
	}
	_, err = p.expect(";")
	return result, err
}

func (p *parser) parseFile() (*idlFile, error) {
	result := &idlFile{}
	for {
		t, err := p.peek()
		if err != nil {
			return nil, err
		}
		if t.kind == tokEOF {
			return result, nil
		}
		attrs, err := p.parseAttributes()
		if err != nil {
			return nil, err
		}
		kind, err := p.expectIdent()
		if err != nil {
			return nil, err
		}
		switch kind {
		case "external":
			def, err := p.parseExternalType(attrs)
			if err != nil {
				return nil, err
			}
			result.ExternalTypes = append(result.ExternalTypes, def)
		case "enum":
			def, err := p.parseEnum(attrs)
			if err != nil {
				return nil, err
			}
			result.Enums = append(result.Enums, def)
		case "dictionary":
			def, err := p.parseDictionary(attrs)
			if err != nil {
				return nil, err
			}
			result.Dictionaries = append(result.Dictionaries, def)
		case "callback":
			def, err := p.parseCallback(attrs)
			if err != nil {
				return nil, err
			}
			result.Callbacks = append(result.Callbacks, def)
		case "interface":
			def, err := p.parseInterface(attrs)
			if err != nil {
				return nil, err
			}
			result.Interfaces = append(result.Interfaces, def)
		default:
			return nil, p.errorf(t, "unsupported top-level declaration %q", kind)
		}
	}
}

func mergeIDL(dst, src *idlFile) {
	dst.ExternalTypes = append(dst.ExternalTypes, src.ExternalTypes...)
	dst.Enums = append(dst.Enums, src.Enums...)
	dst.Dictionaries = append(dst.Dictionaries, src.Dictionaries...)
	dst.Callbacks = append(dst.Callbacks, src.Callbacks...)
	dst.Interfaces = append(dst.Interfaces, src.Interfaces...)
}

type model struct {
	IDL           *idlFile
	ExternalTypes map[string]externalTypeDef
	Enums         map[string]enumDef
	Dictionaries  map[string]dictionaryDef
	Callbacks     map[string]callbackDef
	Interfaces    map[string]interfaceDef
	FilterRoots   map[string]bool
	FilterCtx     map[string]bool
}

var knownRoots = map[string]bool{"Chrome": true, "Firefox": true}
var knownContexts = map[string]bool{
	"BackgroundScript": true, "ContentScript": true, "ExtensionPage": true,
	"DevToolsPage": true, "OffscreenDocument": true, "UserScript": true,
}

func newModel(idl *idlFile, filter string) (*model, error) {
	m := &model{
		IDL: idl, ExternalTypes: make(map[string]externalTypeDef), Enums: make(map[string]enumDef),
		Dictionaries: make(map[string]dictionaryDef), Callbacks: make(map[string]callbackDef), Interfaces: make(map[string]interfaceDef),
		FilterRoots: make(map[string]bool), FilterCtx: make(map[string]bool),
	}
	for _, external := range idl.ExternalTypes {
		if _, exists := m.ExternalTypes[external.Name]; exists {
			return nil, fmt.Errorf("duplicate external type %s", external.Name)
		}
		m.ExternalTypes[external.Name] = external
	}
	for _, enum := range idl.Enums {
		if _, exists := m.Enums[enum.Name]; exists {
			return nil, fmt.Errorf("duplicate enum %s", enum.Name)
		}
		if _, _, err := splitTags(enum.Tags); err != nil {
			return nil, fmt.Errorf("enum %s: %w", enum.Name, err)
		}
		for _, value := range enum.Values {
			if _, _, err := splitTags(value.Tags); err != nil {
				return nil, fmt.Errorf("enum %s value %s: %w", enum.Name, value.Name, err)
			}
		}
		m.Enums[enum.Name] = enum
	}
	for _, d := range idl.Dictionaries {
		if _, exists := m.Dictionaries[d.Name]; exists {
			return nil, fmt.Errorf("duplicate dictionary %s", d.Name)
		}
		if _, _, err := splitTags(d.Tags); err != nil {
			return nil, fmt.Errorf("dictionary %s: %w", d.Name, err)
		}
		fieldNames := make(map[string]bool)
		for _, field := range d.Fields {
			if fieldNames[field.Name] {
				return nil, fmt.Errorf("duplicate field %s in dictionary %s", field.Name, d.Name)
			}
			fieldNames[field.Name] = true
			if _, _, err := splitTags(field.Tags); err != nil {
				return nil, fmt.Errorf("dictionary %s field %s: %w", d.Name, field.Name, err)
			}
		}
		m.Dictionaries[d.Name] = d
	}
	for _, c := range idl.Callbacks {
		if existing, exists := m.Callbacks[c.Name]; exists {
			if existing.ReturnVoid != c.ReturnVoid || existing.Return.key() != c.Return.key() || len(existing.Params) != len(c.Params) {
				return nil, fmt.Errorf("conflicting callback %s", c.Name)
			}
			for i := range c.Params {
				if existing.Params[i].Type.key() != c.Params[i].Type.key() || existing.Params[i].Optional != c.Params[i].Optional {
					return nil, fmt.Errorf("conflicting callback %s", c.Name)
				}
			}
			continue
		}
		m.Callbacks[c.Name] = c
	}
	for i := range idl.Interfaces {
		iface := &idl.Interfaces[i]
		if _, exists := m.Interfaces[iface.Name]; exists {
			return nil, fmt.Errorf("duplicate interface %s", iface.Name)
		}
		for eventIndex := range iface.Events {
			event := &iface.Events[eventIndex]
			if event.Callback == "" {
				continue
			}
			callback, ok := m.Callbacks[event.Callback]
			if !ok {
				return nil, fmt.Errorf("event %s.%s references unknown callback %s", iface.Name, event.Name, event.Callback)
			}
			event.Params = callback.Params
		}
		functionIDs := make(map[string]string)
		for _, fn := range iface.Functions {
			if previous, exists := functionIDs[fn.ID]; exists {
				return nil, fmt.Errorf(
					"duplicate function ID %q in interface %s (%s and %s)",
					fn.ID, iface.Name, previous, fn.Name)
			}
			functionIDs[fn.ID] = fn.Name
		}
		m.Interfaces[iface.Name] = *iface
	}
	if filter != "" {
		for _, raw := range strings.Split(filter, "|") {
			tag := strings.TrimSpace(raw)
			if tag == "" {
				return nil, fmt.Errorf("malformed visibility filter %q", filter)
			}
			if knownRoots[tag] {
				m.FilterRoots[tag] = true
			} else if knownContexts[tag] {
				m.FilterCtx[tag] = true
			} else {
				return nil, fmt.Errorf("unknown visibility tag %q", tag)
			}
		}
	}
	return m, nil
}

func splitTags(tags []string) (map[string]bool, map[string]bool, error) {
	roots := make(map[string]bool)
	contexts := make(map[string]bool)
	for _, tag := range tags {
		if knownRoots[tag] {
			roots[tag] = true
		} else if tag == "Everywhere" {
			for ctx := range knownContexts {
				contexts[ctx] = true
			}
		} else if knownContexts[tag] {
			contexts[tag] = true
		} else {
			return nil, nil, fmt.Errorf("unknown IDL tag %q", tag)
		}
	}
	if len(roots) == 0 {
		roots["Chrome"] = true
		roots["Firefox"] = true
	}
	if len(contexts) == 0 {
		for ctx := range knownContexts {
			contexts[ctx] = true
		}
	}
	return roots, contexts, nil
}

func intersect(a, b map[string]bool) map[string]bool {
	result := make(map[string]bool)
	for key := range a {
		if b[key] {
			result[key] = true
		}
	}
	return result
}

func (m *model) visible(tags, parentTags []string, root string) (bool, error) {
	parentRoots, parentCtx, err := splitTags(parentTags)
	if err != nil {
		return false, err
	}
	memberRoots, memberCtx, err := splitTags(tags)
	if err != nil {
		return false, err
	}
	roots := intersect(parentRoots, memberRoots)
	contexts := intersect(parentCtx, memberCtx)
	if !roots[root] {
		return false, nil
	}
	if len(m.FilterRoots) > 0 && !m.FilterRoots[root] {
		return false, nil
	}
	if len(m.FilterCtx) == 0 {
		return len(contexts) > 0, nil
	}
	for ctx := range contexts {
		if m.FilterCtx[ctx] {
			return true, nil
		}
	}
	return false, nil
}

// definedUnionArm returns the concrete type for the WebExtension schema
// pattern T | undefined. Native Promise<T> already represents undefined by
// resolving without an argument, so this union does not require a C++ variant.
func definedUnionArm(t typeRef) (typeRef, bool) {
	if t.Name != "union" || len(t.Arms) != 2 || t.Nullable {
		return typeRef{}, false
	}
	if t.Arms[0].Name == "undefined" {
		return t.Arms[1], true
	}
	if t.Arms[1].Name == "undefined" {
		return t.Arms[0], true
	}
	return typeRef{}, false
}

func (m *model) cppViewType(t typeRef) string {
	if arm, ok := definedUnionArm(t); ok {
		return m.cppViewType(arm)
	}
	var base string
	switch t.Name {
	case "void":
		base = "void"
	case "boolean":
		base = "bool"
	case "integer":
		base = "int"
	case "number":
		base = "double"
	case "string":
		base = "std::string"
	case "any", "object":
		base = "rfl::Generic"
	case "undefined", "union":
		base = "rfl::Generic"
	case "sequence":
		base = "std::vector<" + m.cppViewType(*t.Elem) + ">"
	case "Promise":
		base = m.cppViewType(*t.Elem)
	default:
		if _, ok := m.ExternalTypes[t.Name]; ok {
			base = "rfl::Generic"
		} else if _, ok := m.Callbacks[t.Name]; ok {
			base = "rfl::Generic"
		} else if _, ok := m.Enums[t.Name]; ok {
			base = "std::string"
		} else if iface, ok := m.Interfaces[t.Name]; ok && iface.Global == "" {
			base = "rfl::Generic"
		} else {
			base = t.Name
		}
	}
	if t.Nullable {
		return "Nullable<" + base + ">"
	}
	return base
}

func (m *model) cppStorageType(t typeRef) string {
	if arm, ok := definedUnionArm(t); ok {
		return m.cppStorageType(arm)
	}
	var base string
	switch t.Name {
	case "sequence":
		base = "std::vector<" + m.cppStorageType(*t.Elem) + ">"
	case "Promise":
		base = m.cppStorageType(*t.Elem)
	default:
		if _, ok := m.Dictionaries[t.Name]; ok {
			base = t.Name + "Storage"
		} else {
			base = m.cppViewType(typeRef{Name: t.Name, Elem: t.Elem})
		}
	}
	if t.Nullable {
		return "NullableStorage<" + base + ">"
	}
	return base
}

func (m *model) isDictionarySequence(t typeRef) bool {
	if t.Nullable || t.Name != "sequence" || t.Elem == nil || t.Elem.Nullable {
		return false
	}
	_, ok := m.Dictionaries[t.Elem.Name]
	return ok
}

func (m *model) isDictionary(t typeRef) bool {
	if t.Nullable {
		return false
	}
	_, ok := m.Dictionaries[t.Name]
	return ok
}

func safeIdent(value string) string {
	re := regexp.MustCompile(`[^A-Za-z0-9_]`)
	value = re.ReplaceAllString(value, "_")
	if value == "" || (value[0] >= '0' && value[0] <= '9') {
		value = "_" + value
	}
	return value
}

func lowerFirst(value string) string {
	if value == "" {
		return value
	}
	return strings.ToLower(value[:1]) + value[1:]
}

func isCallback(t typeRef, callbacks map[string]callbackDef) bool {
	_, ok := callbacks[t.Name]
	return ok
}

func transportParams(fn functionDef, callbacks map[string]callbackDef) []parameter {
	var result []parameter
	for _, param := range fn.Params {
		if !isCallback(param.Type, callbacks) {
			result = append(result, param)
		}
	}
	return result
}

func callbackParam(fn functionDef, callbacks map[string]callbackDef) *parameter {
	for i := range fn.Params {
		if isCallback(fn.Params[i].Type, callbacks) {
			return &fn.Params[i]
		}
	}
	return nil
}

func nativeReturn(fn functionDef, callbacks map[string]callbackDef) typeRef {
	if fn.Return.Name == "Promise" && fn.Return.Elem != nil {
		if arm, ok := definedUnionArm(*fn.Return.Elem); ok {
			return arm
		}
		return *fn.Return.Elem
	}
	if cbParam := callbackParam(fn, callbacks); cbParam != nil {
		cb := callbacks[cbParam.Type.Name]
		if len(cb.Params) > 0 {
			return cb.Params[0].Type
		}
	}
	return typeRef{Name: "void"}
}

type nativeBinding struct {
	Interface   string
	Target      string
	Method      string
	Return      typeRef
	Params      []parameter
	Contexts    map[string]bool
	Permissions []string
}

var securityProfiles = []struct {
	Name string
	Tag  string
}{
	{Name: "BackgroundScript", Tag: "BackgroundScript"},
	{Name: "ExtensionPage", Tag: "ExtensionPage"},
	{Name: "ContentScript", Tag: "ContentScript"},
}

func contextsFor(tags, parentTags []string) (map[string]bool, error) {
	_, parentContexts, err := splitTags(parentTags)
	if err != nil {
		return nil, err
	}
	_, memberContexts, err := splitTags(tags)
	if err != nil {
		return nil, err
	}
	return intersect(parentContexts, memberContexts), nil
}

func securityProfileCondition(contexts map[string]bool) string {
	var conditions []string
	for _, profile := range securityProfiles {
		if contexts[profile.Tag] {
			conditions = append(conditions, "security_profile == SecurityProfile::"+profile.Name)
		}
	}
	if len(conditions) == 0 {
		return "false"
	}
	return strings.Join(conditions, " || ")
}

func nativePermissionCondition(permissions []string) string {
	if len(permissions) == 0 {
		return "true"
	}
	quoted := make([]string, 0, len(permissions))
	for _, permission := range permissions {
		quoted = append(quoted, strconv.Quote(permission))
	}
	return "has_required_permissions(permissions, {" + strings.Join(quoted, ", ") + "})"
}

func sameSecurityProfiles(a, b map[string]bool) bool {
	for _, profile := range securityProfiles {
		if a[profile.Tag] != b[profile.Tag] {
			return false
		}
	}
	return true
}

func (m *model) nativeBindings() ([]nativeBinding, error) {
	var result []nativeBinding
	seen := make(map[string]int)
	for _, iface := range m.IDL.Interfaces {
		for _, fn := range iface.Functions {
			if fn.Impl.Kind != "Native" {
				continue
			}
			contexts, err := contextsFor(fn.Tags, iface.Tags)
			if err != nil {
				return nil, fmt.Errorf("function %s.%s: %w", iface.Name, fn.Name, err)
			}
			key := iface.Name + ":" + fn.Impl.Target
			params := transportParams(fn, m.Callbacks)
			ret := nativeReturn(fn, m.Callbacks)
			if idx, ok := seen[key]; ok {
				existing := result[idx]
				if strings.Join(existing.Permissions, "\x00") != strings.Join(fn.Permissions, "\x00") {
					return nil, fmt.Errorf("native binding %s has incompatible permissions", key)
				}
				if !sameSecurityProfiles(existing.Contexts, contexts) {
					return nil, fmt.Errorf("native binding %s has incompatible security profiles", key)
				}
				if existing.Return.key() != ret.key() || len(existing.Params) != len(params) {
					return nil, fmt.Errorf("native binding %s has incompatible overloads", key)
				}
				for i := range params {
					if existing.Params[i].Name != params[i].Name || existing.Params[i].Type.key() != params[i].Type.key() {
						return nil, fmt.Errorf("native binding %s has incompatible parameters", key)
					}
					// A native target shared by public overloads receives the most
					// permissive representation. Root-specific required overloads
					// still guarantee a non-null value in JavaScript, while another
					// overload may omit the same argument.
					existing.Params[i].Optional = existing.Params[i].Optional || params[i].Optional
				}
				result[idx] = existing
				continue
			}
			method := safeIdent(fn.Impl.Target)
			seen[key] = len(result)
			result = append(result, nativeBinding{Interface: iface.Name, Target: fn.Impl.Target, Method: method, Return: ret, Params: params, Contexts: contexts, Permissions: fn.Permissions})
		}
	}
	return result, nil
}

func generateHeader(m *model) (string, error) {
	bindings, err := m.nativeBindings()
	if err != nil {
		return "", err
	}
	byInterface := make(map[string][]nativeBinding)
	for _, binding := range bindings {
		byInterface[binding.Interface] = append(byInterface[binding.Interface], binding)
	}
	var sb strings.Builder
	sb.WriteString(`#pragma once

#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
#include <rfl/json.hpp>
#include "core/webkit_bridge/API/bridge.h"

namespace WebExtensions::v2 {

enum class SecurityProfile {
    BackgroundScript,
    ExtensionPage,
    ContentScript,
};

struct PolyfillConfiguration {
    SecurityProfile security_profile = SecurityProfile::ExtensionPage;
    std::vector<std::string> permissions;
};

`)
	for _, d := range m.IDL.Dictionaries {
		fmt.Fprintf(&sb, "struct %s;\nstruct %sStorage;\n", d.Name, d.Name)
	}
	sb.WriteString("\n")
	for _, d := range m.IDL.Dictionaries {
		fmt.Fprintf(&sb, "%s from_storage(const %sStorage& storage);\n", d.Name, d.Name)
		fmt.Fprintf(&sb, "%sStorage to_storage(const %s& value);\n", d.Name, d.Name)
	}
	sb.WriteString(`
template<typename T>
struct Nullable {
    bool is_null = false;
    const T* value = nullptr;
};

template<typename T>
struct NullableStorage {
    bool is_null = false;
    std::optional<T> value;
};

`)
	for _, d := range m.IDL.Dictionaries {
		fmt.Fprintf(&sb, "struct %s {\n", d.Name)
		for _, field := range d.Fields {
			viewType := m.cppViewType(field.Type)
			if field.Optional {
				fmt.Fprintf(&sb, "    const %s* %s = nullptr;\n", viewType, field.Name)
			} else {
				fmt.Fprintf(&sb, "    const %s& %s;\n", viewType, field.Name)
			}
		}
		fmt.Fprintln(&sb, "};")
		fmt.Fprintf(&sb, "struct %sStorage {\n", d.Name)
		for _, field := range d.Fields {
			storageType := m.cppStorageType(field.Type)
			if field.Optional {
				fmt.Fprintf(&sb, "    std::optional<%s> %s;\n", storageType, field.Name)
			} else {
				fmt.Fprintf(&sb, "    %s %s;\n", storageType, field.Name)
			}
			if m.isDictionarySequence(field.Type) {
				viewType := m.cppViewType(field.Type)
				if field.Optional {
					fmt.Fprintf(&sb, "    mutable rfl::Skip<std::optional<%s>> __view_%s = {};\n", viewType, field.Name)
				} else {
					fmt.Fprintf(&sb, "    mutable rfl::Skip<%s> __view_%s = {};\n", viewType, field.Name)
				}
			} else if m.isDictionary(field.Type) {
				fmt.Fprintf(&sb, "    mutable rfl::Skip<std::optional<%s>> __view_%s = {};\n", m.cppViewType(field.Type), field.Name)
			}
		}
		fmt.Fprintln(&sb, "};")
		fmt.Fprintln(&sb)
	}

	sb.WriteString(`template<typename T>
struct StorageConverter {
    using Type = T;
    static Type convert(const T& value) { return value; }
};

`)
	for _, d := range m.IDL.Dictionaries {
		fmt.Fprintf(&sb, "template<>\nstruct StorageConverter<%s> {\n", d.Name)
		fmt.Fprintf(&sb, "    using Type = %sStorage;\n", d.Name)
		fmt.Fprintf(&sb, "    static Type convert(const %s& value) { return to_storage(value); }\n", d.Name)
		fmt.Fprintln(&sb, "};")
		fmt.Fprintln(&sb)
	}
	sb.WriteString(`template<typename T>
struct StorageConverter<std::vector<T>> {
    using ItemStorage = typename StorageConverter<T>::Type;
    using Type = std::vector<ItemStorage>;
    static Type convert(const std::vector<T>& values) {
        Type result;
        result.reserve(values.size());
        for(const auto& value : values) {
            result.push_back(StorageConverter<T>::convert(value));
        }
        return result;
    }
};

template<typename T>
typename StorageConverter<T>::Type to_storage_value(const T& value) {
    return StorageConverter<T>::convert(value);
}

`)
	for _, d := range m.IDL.Dictionaries {
		fmt.Fprintf(&sb, "inline %s\nfrom_storage(const %sStorage& storage)\n{\n", d.Name, d.Name)
		for _, field := range d.Fields {
			if m.isDictionarySequence(field.Type) && field.Optional {
				fmt.Fprintf(&sb, "    if(storage.%s) {\n", field.Name)
				fmt.Fprintf(&sb, "        storage.__view_%s.get().emplace();\n", field.Name)
				fmt.Fprintf(&sb, "        storage.__view_%s.get()->reserve(storage.%s->size());\n", field.Name, field.Name)
				fmt.Fprintf(&sb, "        for(const auto& item : *storage.%s) storage.__view_%s.get()->push_back(from_storage(item));\n", field.Name, field.Name)
				fmt.Fprintln(&sb, "    } else {")
				fmt.Fprintf(&sb, "        storage.__view_%s.get().reset();\n", field.Name)
				fmt.Fprintln(&sb, "    }")
			} else if m.isDictionarySequence(field.Type) {
				fmt.Fprintf(&sb, "    storage.__view_%s.get().clear();\n", field.Name)
				fmt.Fprintf(&sb, "    storage.__view_%s.get().reserve(storage.%s.size());\n", field.Name, field.Name)
				fmt.Fprintf(&sb, "    for(const auto& item : storage.%s) storage.__view_%s.get().push_back(from_storage(item));\n", field.Name, field.Name)
			} else if m.isDictionary(field.Type) && field.Optional {
				fmt.Fprintf(&sb, "    if(storage.%s) storage.__view_%s.get().emplace(from_storage(*storage.%s));\n", field.Name, field.Name, field.Name)
				fmt.Fprintf(&sb, "    else storage.__view_%s.get().reset();\n", field.Name)
			} else if m.isDictionary(field.Type) {
				fmt.Fprintf(&sb, "    storage.__view_%s.get().emplace(from_storage(storage.%s));\n", field.Name, field.Name)
			}
		}
		fmt.Fprintln(&sb, "    return {")
		for _, field := range d.Fields {
			if m.isDictionary(field.Type) && field.Optional {
				fmt.Fprintf(&sb, "        .%s = storage.__view_%s.get() ? &storage.__view_%s.get().value() : nullptr,\n", field.Name, field.Name, field.Name)
			} else if m.isDictionary(field.Type) {
				fmt.Fprintf(&sb, "        .%s = storage.__view_%s.get().value(),\n", field.Name, field.Name)
			} else if m.isDictionarySequence(field.Type) && field.Optional {
				fmt.Fprintf(&sb, "        .%s = storage.__view_%s.get() ? &storage.__view_%s.get().value() : nullptr,\n", field.Name, field.Name, field.Name)
			} else if m.isDictionarySequence(field.Type) {
				fmt.Fprintf(&sb, "        .%s = storage.__view_%s.get(),\n", field.Name, field.Name)
			} else if field.Optional {
				fmt.Fprintf(&sb, "        .%s = storage.%s ? &storage.%s.value() : nullptr,\n", field.Name, field.Name, field.Name)
			} else {
				fmt.Fprintf(&sb, "        .%s = storage.%s,\n", field.Name, field.Name)
			}
		}
		fmt.Fprintln(&sb, "    };")
		fmt.Fprintln(&sb, "}")
		fmt.Fprintln(&sb)
		fmt.Fprintf(&sb, "inline %sStorage\nto_storage(const %s& value)\n{\n", d.Name, d.Name)
		fmt.Fprintln(&sb, "    return {")
		for _, field := range d.Fields {
			if field.Optional {
				fmt.Fprintf(&sb, "        .%s = value.%s ? std::optional<%s>(to_storage_value(*value.%s)) : std::nullopt,\n", field.Name, field.Name, m.cppStorageType(field.Type), field.Name)
			} else {
				fmt.Fprintf(&sb, "        .%s = to_storage_value(value.%s),\n", field.Name, field.Name)
			}
		}
		fmt.Fprintln(&sb, "    };")
		fmt.Fprintln(&sb, "}")
		fmt.Fprintln(&sb)
	}

	sb.WriteString(`template<typename T>
class Promise {
public:
    Promise(std::string extension_id, orion::bridge::WebViewBridge& webview, int promise, std::string script_world, SecurityProfile security_profile)
        : m_extension_id(std::move(extension_id)), m_webview(webview.weak_from_this()), m_promise(promise), m_script_world(std::move(script_world)), m_security_profile(security_profile) {}
    const std::string& extension_id() const { return m_extension_id; }
    std::weak_ptr<orion::bridge::WebViewBridge> webview() const { return m_webview; }
    const std::string& script_world() const { return m_script_world; }
    SecurityProfile security_profile() const { return m_security_profile; }
    void resolve(const T& value) const {
        auto stored_arguments = std::make_tuple(to_storage_value(value));
        struct Payload { int promise; bool resolve; decltype(stored_arguments) arguments; };
        send_payload(rfl::json::write(Payload{m_promise, true, std::move(stored_arguments)}));
    }
    void resolve() const { handle_empty(true); }
    void reject() const { handle_empty(false); }
    void reject(const std::string& message) const {
        struct Payload { int promise; bool resolve; std::tuple<std::string> arguments; };
        send_payload(rfl::json::write(Payload{m_promise, false, {message}}));
    }
private:
    void handle_empty(bool resolve) const {
        struct Payload { int promise; bool resolve; std::tuple<> arguments; };
        send_payload(rfl::json::write(Payload{m_promise, resolve, {}}));
    }
    void send_payload(const std::string& payload_json) const {
        if(auto webview = m_webview.lock()) webview->web_view_call("__bridge_v2__.handlePromise(...arguments)", payload_json, m_script_world);
    }
    std::string m_extension_id;
    std::weak_ptr<orion::bridge::WebViewBridge> m_webview;
    int m_promise = 0;
    std::string m_script_world;
    SecurityProfile m_security_profile;
};

template<>
class Promise<void> {
public:
    Promise(std::string extension_id, orion::bridge::WebViewBridge& webview, int promise, std::string script_world, SecurityProfile security_profile)
        : m_extension_id(std::move(extension_id)), m_webview(webview.weak_from_this()), m_promise(promise), m_script_world(std::move(script_world)), m_security_profile(security_profile) {}
    const std::string& extension_id() const { return m_extension_id; }
    std::weak_ptr<orion::bridge::WebViewBridge> webview() const { return m_webview; }
    const std::string& script_world() const { return m_script_world; }
    SecurityProfile security_profile() const { return m_security_profile; }
    void resolve() const { handle_empty(true); }
    void reject() const { handle_empty(false); }
    void reject(const std::string& message) const {
        struct Payload { int promise; bool resolve; std::tuple<std::string> arguments; };
        send_payload(rfl::json::write(Payload{m_promise, false, {message}}));
    }
private:
    void handle_empty(bool resolve) const {
        struct Payload { int promise; bool resolve; std::tuple<> arguments; };
        send_payload(rfl::json::write(Payload{m_promise, resolve, {}}));
    }
    void send_payload(const std::string& payload_json) const {
        if(auto webview = m_webview.lock()) webview->web_view_call("__bridge_v2__.handlePromise(...arguments)", payload_json, m_script_world);
    }
    std::string m_extension_id;
    std::weak_ptr<orion::bridge::WebViewBridge> m_webview;
    int m_promise = 0;
    std::string m_script_world;
    SecurityProfile m_security_profile;
};

template<typename... Args>
class Listener {
public:
    struct Registration {
        std::string arguments_json;
        std::string extension_id;
        std::string script_world;
        SecurityProfile security_profile;
    };
    void addListener(orion::bridge::WebViewBridge& webview, int listener, std::string registration_arguments_json, std::string extension_id, std::string script_world, SecurityProfile security_profile) {
        const ListenerKey key{script_world, listener};
        m_listeners[webview.weak_from_this()].insert_or_assign(key, Registration{
            std::move(registration_arguments_json),
            std::move(extension_id),
            std::move(script_world),
            security_profile
        });
    }
    void removeListener(orion::bridge::WebViewBridge& webview, int listener, const std::string& script_world = {}) {
        auto it = m_listeners.find(webview.weak_from_this());
        if(it == m_listeners.end()) return;
        it->second.erase(ListenerKey{script_world, listener});
        if(it->second.empty()) m_listeners.erase(it);
    }
    const std::string* registration_arguments(orion::bridge::WebViewBridge& webview, int listener, const std::string& script_world = {}) const {
        auto webview_it = m_listeners.find(webview.weak_from_this());
        if(webview_it == m_listeners.end()) return nullptr;
        auto listener_it = webview_it->second.find(ListenerKey{script_world, listener});
        return listener_it == webview_it->second.end() ? nullptr : &listener_it->second.arguments_json;
    }
    void notify_one(orion::bridge::WebViewBridge& webview, int listener, const std::string& script_world, const Args&... args) {
        auto stored_arguments = std::make_tuple(to_storage_value(args)...);
        struct Payload { int listener; decltype(stored_arguments) arguments; };
        const std::string payload_json = rfl::json::write(Payload{listener, std::move(stored_arguments)});
        webview.web_view_call("__bridge_v2__.handleNotify(...arguments)", payload_json, script_world);
    }
    void notify(const Args&... args) {
        for(auto webview_it = m_listeners.begin(); webview_it != m_listeners.end();) {
            auto webview = webview_it->first.lock();
            if(!webview) {
                webview_it = m_listeners.erase(webview_it);
                continue;
            }
            for(const auto& [key, registration] : webview_it->second) {
                notify_one(*webview, key.second, registration.script_world, args...);
            }
            ++webview_it;
        }
    }
    size_t notify_extension(const std::string& extension_id, const Args&... args) {
        return notify_extension_if(extension_id, [](orion::bridge::WebViewBridge&, const Registration&) { return true; }, args...);
    }
    template<typename Predicate>
    size_t count_extension_if(const std::string& extension_id, const Predicate& predicate) {
        size_t count = 0;
        for(auto webview_it = m_listeners.begin(); webview_it != m_listeners.end();) {
            auto webview = webview_it->first.lock();
            if(!webview) {
                webview_it = m_listeners.erase(webview_it);
                continue;
            }
            for(const auto& [key, registration] : webview_it->second) {
                if(registration.extension_id == extension_id && predicate(*webview, registration)) ++count;
            }
            ++webview_it;
        }
        return count;
    }
    template<typename Predicate>
    size_t notify_extension_if(const std::string& extension_id, const Predicate& predicate, const Args&... args) {
        size_t notified = 0;
        for(auto webview_it = m_listeners.begin(); webview_it != m_listeners.end();) {
            auto webview = webview_it->first.lock();
            if(!webview) {
                webview_it = m_listeners.erase(webview_it);
                continue;
            }
            for(const auto& [key, registration] : webview_it->second) {
                if(registration.extension_id != extension_id || !predicate(*webview, registration)) continue;
                notify_one(*webview, key.second, registration.script_world, args...);
                ++notified;
            }
            ++webview_it;
        }
        return notified;
    }
    template<typename Predicate>
    size_t count_if(const Predicate& predicate) {
        size_t count = 0;
        for(auto webview_it = m_listeners.begin(); webview_it != m_listeners.end();) {
            auto webview = webview_it->first.lock();
            if(!webview) {
                webview_it = m_listeners.erase(webview_it);
                continue;
            }
            if(predicate(*webview)) count += webview_it->second.size();
            ++webview_it;
        }
        return count;
    }
    template<typename Predicate>
    size_t notify_if(const Predicate& predicate, const Args&... args) {
        size_t notified = 0;
        for(auto webview_it = m_listeners.begin(); webview_it != m_listeners.end();) {
            auto webview = webview_it->first.lock();
            if(!webview) {
                webview_it = m_listeners.erase(webview_it);
                continue;
            }
            if(!predicate(*webview)) {
                ++webview_it;
                continue;
            }
            for(const auto& [key, registration] : webview_it->second) {
                notify_one(*webview, key.second, registration.script_world, args...);
                ++notified;
            }
            ++webview_it;
        }
        return notified;
    }
    void operator()(const Args&... args) { notify(args...); }
private:
    using ListenerKey = std::pair<std::string, int>;
    using WebViewRef = std::weak_ptr<orion::bridge::WebViewBridge>;
    std::map<WebViewRef, std::map<ListenerKey, Registration>, std::owner_less<WebViewRef>> m_listeners;
};

`)
	for _, iface := range m.IDL.Interfaces {
		fmt.Fprintf(&sb, "class %s;\n", iface.Name)
	}
	sb.WriteString("\nstruct Implementations {\n")
	for _, iface := range m.IDL.Interfaces {
		fmt.Fprintf(&sb, "    %s* %s = nullptr;\n", iface.Name, lowerFirst(iface.Name))
	}
	sb.WriteString("};\n\ntemplate<typename T>\nImplementations make_implementations(T* instance) {\n    Implementations impls{};\n")
	for _, iface := range m.IDL.Interfaces {
		fmt.Fprintf(&sb, "    if constexpr (std::is_base_of_v<%s, T>) impls.%s = static_cast<%s*>(instance);\n", iface.Name, lowerFirst(iface.Name), iface.Name)
	}
	sb.WriteString("    return impls;\n}\n\n")
	for _, iface := range m.IDL.Interfaces {
		fmt.Fprintf(&sb, "class %s {\npublic:\n", iface.Name)
		for _, binding := range byInterface[iface.Name] {
			fmt.Fprintf(&sb, "    virtual void %s(Promise<%s> promise", binding.Method, m.cppViewType(binding.Return))
			for _, param := range binding.Params {
				if param.Optional {
					fmt.Fprintf(&sb, ", const %s* %s", m.cppViewType(param.Type), param.Name)
				} else {
					fmt.Fprintf(&sb, ", const %s& %s", m.cppViewType(param.Type), param.Name)
				}
			}
			fmt.Fprintf(&sb, ") { promise.reject(\"%s.%s is not implemented\"); }\n", iface.Name, binding.Target)
		}
		for _, event := range iface.Events {
			if event.Impl.Kind != "Native" {
				continue
			}
			var types []string
			for _, param := range event.Params {
				types = append(types, m.cppViewType(param.Type))
			}
			fmt.Fprintf(&sb, "    Listener<%s> %s;\n", strings.Join(types, ", "), event.Name)
		}
		fmt.Fprintln(&sb, "};")
		fmt.Fprintln(&sb)
	}
	sb.WriteString(`bool native_call_bridge_received(int promise, const std::string& clazz, const std::string& function, const std::string& arguments_json, orion::bridge::WebViewBridge& webview, const std::string& extension_id, const std::string& script_world, SecurityProfile security_profile, const std::vector<std::string>& permissions, const Implementations& impls, std::string& error);
bool native_event_bridge_received(int listener, const std::string& clazz, const std::string& event, const std::string& command, const std::string& registration_arguments_json, orion::bridge::WebViewBridge& webview, const std::string& extension_id, const std::string& script_world, SecurityProfile security_profile, const std::vector<std::string>& permissions, const Implementations& impls, std::string& error);
std::string native_bridge_script(const PolyfillConfiguration& configuration);

} // namespace WebExtensions::v2
`)
	return sb.String(), nil
}

func jsTypeDescriptor(t typeRef, m *model, root string) (string, error) {
	if t.Nullable {
		nonNull := t
		nonNull.Nullable = false
		descriptor, err := jsTypeDescriptor(nonNull, m, root)
		if err != nil {
			return "", err
		}
		return fmt.Sprintf(`{kind:"nullable",inner:%s}`, descriptor), nil
	}
	if t.Name == "union" {
		arms := make([]string, 0, len(t.Arms))
		for _, arm := range t.Arms {
			descriptor, err := jsTypeDescriptor(arm, m, root)
			if err != nil {
				return "", err
			}
			arms = append(arms, descriptor)
		}
		return fmt.Sprintf(`{kind:"union",arms:[%s]}`, strings.Join(arms, ",")), nil
	}
	if _, ok := m.Callbacks[t.Name]; ok {
		return `{kind:"callback"}`, nil
	}
	if enum, ok := m.Enums[t.Name]; ok {
		var values []string
		for _, value := range enum.Values {
			visible, err := m.visibleInProjection(value.Tags, enum.Tags, root)
			if err != nil {
				return "", err
			}
			if visible {
				values = append(values, strconv.Quote(value.Value))
			}
		}
		return fmt.Sprintf(`{kind:"enum",values:[%s]}`, strings.Join(values, ",")), nil
	}
	if _, ok := m.ExternalTypes[t.Name]; ok {
		return `{kind:"object"}`, nil
	}
	if _, ok := m.Interfaces[t.Name]; ok {
		return `{kind:"object"}`, nil
	}
	if dict, ok := m.Dictionaries[t.Name]; ok {
		var fields []string
		knownFields := make([]string, 0, len(dict.Fields))
		for _, field := range dict.Fields {
			knownFields = append(knownFields, strconv.Quote(field.Name))
			visible, err := m.visibleInProjection(field.Tags, dict.Tags, root)
			if err != nil {
				return "", err
			}
			if !visible {
				continue
			}
			descriptor, err := jsTypeDescriptor(field.Type, m, root)
			if err != nil {
				return "", err
			}
			fields = append(fields, fmt.Sprintf("%s:{type:%s,optional:%t,hasDefault:%t,defaultValue:%s}", strconv.Quote(field.Name), descriptor, field.Optional, field.Default.Set, field.Default.js()))
		}
		return fmt.Sprintf(`{kind:"dictionary",fields:{%s},known:[%s],additional:%t}`, strings.Join(fields, ","), strings.Join(knownFields, ","), dict.AdditionalProperties), nil
	}
	switch t.Name {
	case "boolean":
		return `{kind:"boolean"}`, nil
	case "integer", "number":
		return `{kind:"number"}`, nil
	case "string":
		return `{kind:"string"}`, nil
	case "any":
		return `{kind:"any"}`, nil
	case "object":
		return `{kind:"object"}`, nil
	case "undefined":
		return `{kind:"undefined"}`, nil
	case "sequence":
		item, err := jsTypeDescriptor(*t.Elem, m, root)
		if err != nil {
			return "", err
		}
		return fmt.Sprintf(`{kind:"sequence",item:%s}`, item), nil
	default:
		return `{kind:"object"}`, nil
	}
}

func jsParameterDescriptor(param parameter, m *model, root string) (string, error) {
	descriptor, err := jsTypeDescriptor(param.Type, m, root)
	if err != nil {
		return "", err
	}
	maxLength := "null"
	if value, ok := param.Attrs.Values["MaxLength"]; ok {
		maxLength = value.Text
	}
	pattern := "null"
	if value, ok := param.Attrs.Values["Pattern"]; ok {
		pattern = strconv.Quote(value.Text)
	}
	undefinedMissing := false
	if value, ok := param.Attrs.Values["Undefined"]; ok {
		undefinedMissing = value.Text == "Missing"
	}
	nullMissing := false
	if value, ok := param.Attrs.Values["Null"]; ok {
		nullMissing = value.Text == "Missing"
	}
	return fmt.Sprintf(
		`{name:%s,type:%s,optional:%t,hasDefault:%t,defaultValue:%s,maxLength:%s,pattern:%s,undefinedMissing:%t,nullMissing:%t}`,
		strconv.Quote(param.Name), descriptor, param.Optional, param.Default.Set, param.Default.js(),
		maxLength, pattern, undefinedMissing, nullMissing), nil
}

func functionResultType(fn functionDef, callbacks map[string]callbackDef) (typeRef, bool) {
	if fn.Return.Name == "Promise" && fn.Return.Elem != nil && fn.Return.Elem.Name != "void" {
		return *fn.Return.Elem, true
	}
	if !fn.ReturnVoid && fn.Return.Name != "Promise" {
		return fn.Return, true
	}
	if cbParam := callbackParam(fn, callbacks); cbParam != nil {
		callback := callbacks[cbParam.Type.Name]
		if len(callback.Params) > 0 {
			return callback.Params[0].Type, true
		}
	}
	return typeRef{}, false
}

const combinedProjection = ""

func (m *model) visibleInProjection(tags, parentTags []string, projection string) (bool, error) {
	if projection != combinedProjection {
		return m.visible(tags, parentTags, projection)
	}
	for _, root := range []string{"Chrome", "Firefox"} {
		visible, err := m.visible(tags, parentTags, root)
		if err != nil {
			return false, err
		}
		if visible {
			return true, nil
		}
	}
	return false, nil
}

func (m *model) parametersForProjection(params []parameter, parentTags []string, projection string) ([]parameter, error) {
	result := make([]parameter, 0, len(params))
	for _, param := range params {
		visible, err := m.visibleInProjection(param.Attrs.Tags, parentTags, projection)
		if err != nil {
			return nil, err
		}
		if visible {
			result = append(result, param)
		}
	}
	return result, nil
}

func jsReturnKind(fn functionDef) string {
	if fn.Return.Name == "Promise" {
		return "promise"
	}
	if !fn.ReturnVoid {
		return "sync"
	}
	return "void"
}

func (m *model) variantsFor(iface interfaceDef, root string) ([]functionDef, error) {
	var result []functionDef
	for _, fn := range iface.Functions {
		if fn.Internal {
			continue
		}
		visible, err := m.visibleInProjection(fn.Tags, iface.Tags, root)
		if err != nil {
			return nil, err
		}
		if visible {
			result = append(result, fn)
		}
	}
	return result, nil
}

func (m *model) eventsFor(iface interfaceDef, root string) ([]eventDef, error) {
	var result []eventDef
	for _, event := range iface.Events {
		if event.Internal {
			continue
		}
		visible, err := m.visibleInProjection(event.Tags, iface.Tags, root)
		if err != nil {
			return nil, err
		}
		if visible {
			result = append(result, event)
		}
	}
	return result, nil
}

func (m *model) internalEventsFor(iface interfaceDef, root string) ([]eventDef, error) {
	var result []eventDef
	for _, event := range iface.Events {
		if !event.Internal {
			continue
		}
		visible, err := m.visibleInProjection(event.Tags, iface.Tags, root)
		if err != nil {
			return nil, err
		}
		if visible {
			result = append(result, event)
		}
	}
	return result, nil
}

func (m *model) propertiesFor(iface interfaceDef, root string) ([]propertyDef, error) {
	var result []propertyDef
	for _, property := range iface.Properties {
		if property.Internal {
			continue
		}
		visible, err := m.visibleInProjection(property.Tags, iface.Tags, root)
		if err != nil {
			return nil, err
		}
		if visible {
			result = append(result, property)
		}
	}
	return result, nil
}

func (m *model) sharesRootObject(iface interfaceDef) (bool, error) {
	chromeFunctions, err := m.variantsFor(iface, "Chrome")
	if err != nil {
		return false, err
	}
	firefoxFunctions, err := m.variantsFor(iface, "Firefox")
	if err != nil {
		return false, err
	}
	chromeEvents, err := m.eventsFor(iface, "Chrome")
	if err != nil {
		return false, err
	}
	firefoxEvents, err := m.eventsFor(iface, "Firefox")
	if err != nil {
		return false, err
	}
	chromeProperties, err := m.propertiesFor(iface, "Chrome")
	if err != nil {
		return false, err
	}
	firefoxProperties, err := m.propertiesFor(iface, "Firefox")
	if err != nil {
		return false, err
	}
	chromeVisible := len(chromeFunctions)+len(chromeProperties)+len(chromeEvents) > 0
	firefoxVisible := len(firefoxFunctions)+len(firefoxProperties)+len(firefoxEvents) > 0
	return chromeVisible && firefoxVisible, nil
}

func generateJSFragment(
	m *model, selectedInterface string, includePreamble bool, includePostamble bool,
) (string, error) {
	var sb strings.Builder
	if includePreamble {
		sb.WriteString(`(function () {
    const __bridge_v2__ = {
        nativeCallHandler: window.__webextensions_native_call_handler__ || "nativeCallBridge",
        nativeEventHandler: window.__webextensions_native_event_handler__ || "nativeEventBridge",
        promiseRegistry: {},
        nextPromiseId: 1,
        listenerRegistry: {},
        nextListenerId: 1,
        lastError: null,
        implementations: {...(window.__webextensions_v2_implementations__ || {})},
        registerImplementation(name, implementation) {
            if(typeof implementation !== "function") throw new TypeError("WebExtensions v2 implementation must be a function");
            this.implementations[name] = implementation;
            window.__webextensions_v2_implementations__ = window.__webextensions_v2_implementations__ || {};
            window.__webextensions_v2_implementations__[name] = implementation;
        },
        invokeImplementation(name, context) {
            const implementation = this.implementations[name];
            if(!implementation) throw new Error("WebExtensions v2 JavaScript implementation is not registered: " + name);
            return implementation(context);
        },
        postMessage(className, fn, args) {
            return new Promise((resolve, reject) => {
                const promise = this.nextPromiseId++;
                this.promiseRegistry[promise] = {resolve, reject};
                const payload = {version: 2, class: className, function: fn, arguments: JSON.stringify(args), promise};
                window.webkit.messageHandlers[this.nativeCallHandler].postMessage(JSON.stringify(payload));
            });
        },
        parseJson(value) {
            if(typeof value !== "string") return value;
            try { return JSON.parse(value); } catch(error) { console.error("WebExtensions v2: invalid JSON", error); return null; }
        },
        handlePromise(value) {
            const payload = this.parseJson(value);
            if(!payload) return;
            const entry = this.promiseRegistry[payload.promise];
            if(!entry) return;
            delete this.promiseRegistry[payload.promise];
            const args = Array.isArray(payload.arguments) ? payload.arguments : [];
            if(payload.resolve) entry.resolve(...args);
            else entry.reject(new Error(typeof args[0] === "string" ? args[0] : "WebExtension native call failed"));
        },
		addListener(className, eventName, listener, argumentTypes, registrationArguments) {
			const listenerId = this.nextListenerId++;
			this.listenerRegistry[listenerId] = {className, eventName, listener, argumentTypes, registrationArguments};
			window.webkit.messageHandlers[this.nativeEventHandler].postMessage(JSON.stringify({version: 2, class: className, event: eventName, command: "add", listener: listenerId, arguments: JSON.stringify(registrationArguments)}));
        },
        removeListener(className, eventName, listener) {
            const item = Object.entries(this.listenerRegistry).find(([, value]) => value.className === className && value.eventName === eventName && value.listener === listener);
            if(!item) return;
            const listenerId = Number(item[0]);
            delete this.listenerRegistry[listenerId];
            window.webkit.messageHandlers[this.nativeEventHandler].postMessage(JSON.stringify({version: 2, class: className, event: eventName, command: "remove", listener: listenerId}));
        },
		hasListener(className, eventName, listener) {
			return Object.values(this.listenerRegistry).some(value => value.className === className && value.eventName === eventName && value.listener === listener);
		},
		hasListeners(className, eventName) {
			return Object.values(this.listenerRegistry).some(value => value.className === className && value.eventName === eventName);
		},
        handleNotify(value) {
            const payload = this.parseJson(value);
            const entry = payload && this.listenerRegistry[payload.listener];
            if(entry) {
                const args = Array.isArray(payload.arguments) ? payload.arguments : [];
                entry.listener(...args.map((argument, index) => entry.argumentTypes[index] ? this.project(argument, entry.argumentTypes[index]) : argument));
            }
        },
        project(value, type) {
            if(!type || value === undefined || value === null) return value;
            if(type.kind === "nullable") return this.project(value, type.inner);
            if(type.kind === "union") {
                const arm = type.arms.find(candidate => {
                    if(candidate.kind === "dictionary") return value !== null && typeof value === "object" && !Array.isArray(value);
                    if(candidate.kind === "sequence") return Array.isArray(value);
                    return this.typeMatches(value, candidate);
                });
                return arm ? this.project(value, arm) : value;
            }
            if(type.kind === "sequence") {
                return Array.isArray(value) ? value.map(item => this.project(item, type.item)) : value;
            }
            if(type.kind === "dictionary") {
                if(typeof value !== "object" || Array.isArray(value)) return value;
                const result = {};
                if(type.additional) {
                    for(const [name, item] of Object.entries(value)) {
                        if(!type.known.includes(name)) result[name] = item;
                    }
                }
                for(const [name, field] of Object.entries(type.fields)) {
                    if(Object.prototype.hasOwnProperty.call(value, name)) result[name] = this.project(value[name], field.type);
                }
                return result;
            }
            return value;
        },
        normalize(value, type) {
            if(!type || value === undefined || value === null) return value;
            if(type.kind === "nullable") return this.normalize(value, type.inner);
            if(type.kind === "union") {
                const arm = type.arms.find(candidate => this.typeMatches(value, candidate));
                return arm ? this.normalize(value, arm) : value;
            }
            if(type.kind === "sequence") return value.map(item => this.normalize(item, type.item));
            if(type.kind === "dictionary") {
                const result = type.additional ? {...value} : {};
                for(const [name, field] of Object.entries(type.fields)) {
                    if(Object.prototype.hasOwnProperty.call(value, name) && value[name] !== undefined) {
                        result[name] = this.normalize(value[name], field.type);
                    } else if(field.hasDefault) {
                        result[name] = field.defaultValue;
                    }
                }
                return result;
            }
            return value;
        },
        typeMatches(value, type) {
            if(type.kind === "any") return true;
            if(type.kind === "undefined") return value === undefined;
            if(type.kind === "nullable") return value === null || this.typeMatches(value, type.inner);
            if(type.kind === "union") return type.arms.some(arm => this.typeMatches(value, arm));
            if(type.kind === "callback") return typeof value === "function";
            if(type.kind === "string") return typeof value === "string";
            if(type.kind === "enum") return typeof value === "string" && type.values.includes(value);
            if(type.kind === "boolean") return typeof value === "boolean";
            if(type.kind === "number") return typeof value === "number" && Number.isFinite(value);
            if(type.kind === "object") return value !== null && typeof value === "object" && !Array.isArray(value);
            if(type.kind === "sequence") return Array.isArray(value) && value.every(item => this.typeMatches(item, type.item));
            if(type.kind === "dictionary") {
                if(value === null || typeof value !== "object" || Array.isArray(value)) return false;
                for(const [name, field] of Object.entries(type.fields)) {
                    if(!field.optional && !(name in value)) return false;
                    if(name in value && value[name] !== undefined && !this.typeMatches(value[name], field.type)) return false;
                }
                return type.additional || Object.keys(value).every(name => name in type.fields);
            }
            return false;
        },
        paramMatches(value, param) {
            if(!this.typeMatches(value, param.type)) return false;
            if(typeof value === "string") {
                if(param.maxLength !== null && value.length > param.maxLength) return false;
                if(param.pattern && !(new RegExp(param.pattern)).test(value)) return false;
            }
            return true;
        },
        bind(args, params) {
            const memo = new Map();
            const walk = (pi, ai) => {
                const key = pi + ":" + ai;
                if(memo.has(key)) return memo.get(key);
                if(pi === params.length) return ai === args.length ? {values: {}, score: 0} : null;
                const param = params[pi];
                let best = null;
                if(param.optional) {
                    const skipped = walk(pi + 1, ai);
                    if(skipped) {
                        const values = {...skipped.values};
                        if(param.hasDefault) values[param.name] = param.defaultValue;
                        best = {values, score: skipped.score + 10};
                    }
                    const missingValue = ai < args.length
                        && ((args[ai] === undefined && param.undefinedMissing) || (args[ai] === null && param.nullMissing));
                    if(missingValue) {
                        const consumedMissing = walk(pi + 1, ai + 1);
                        if(consumedMissing) {
                            const values = {...consumedMissing.values};
                            if(param.hasDefault) values[param.name] = param.defaultValue;
                            const candidate = {values, score: consumedMissing.score + 1};
                            if(!best || candidate.score < best.score) best = candidate;
                        }
                    }
                }
                if(ai < args.length && this.paramMatches(args[ai], param)) {
                    const consumed = walk(pi + 1, ai + 1);
                    if(consumed) {
                        // A required parameter is a more specific overload match
                        // than an optional parameter consuming the same value.
                        const penalty = (param.type.kind === "any" ? 5 : 0)
                            + (param.optional ? 1 : 0);
                        const candidate = {values: {...consumed.values, [param.name]: this.normalize(args[ai], param.type)}, score: consumed.score + penalty};
                        if(!best || candidate.score < best.score) best = candidate;
                    }
                }
                memo.set(key, best);
                return best;
            };
            return walk(0, 0);
        },
        callbackWithError(callback, error) {
            this.lastError = {message: error && error.message ? error.message : String(error)};
            try { callback(); } finally { this.lastError = null; }
        }
    };

    function createListener(className, eventName, argumentTypes, registrationParams = []) {
        return {
            addListener(listener, ...args) {
                if(typeof listener !== "function") throw new TypeError(className + "." + eventName + ".addListener requires a function");
                const match = __bridge_v2__.bind(args, registrationParams);
                if(!match) throw new TypeError("No v2 event registration signature matches provided arguments");
                __bridge_v2__.addListener(className, eventName, listener, argumentTypes, match.values);
            },
            removeListener(listener) { __bridge_v2__.removeListener(className, eventName, listener); },
            hasListener(listener) { return __bridge_v2__.hasListener(className, eventName, listener); },
            hasListeners() { return __bridge_v2__.hasListeners(className, eventName); }
        };
    }

`)
	}

	for _, iface := range m.IDL.Interfaces {
		if iface.Global == "" || (selectedInterface != "" && iface.Name != selectedInterface) {
			continue
		}
		shared, err := m.sharesRootObject(iface)
		if err != nil {
			return "", err
		}
		var projections []string
		if shared {
			projections = []string{combinedProjection}
		} else {
			for _, root := range []string{"Chrome", "Firefox"} {
				functions, err := m.variantsFor(iface, root)
				if err != nil {
					return "", err
				}
				events, err := m.eventsFor(iface, root)
				if err != nil {
					return "", err
				}
				properties, err := m.propertiesFor(iface, root)
				if err != nil {
					return "", err
				}
				if len(functions)+len(properties)+len(events) > 0 {
					projections = append(projections, root)
				}
			}
		}
		for _, root := range projections {
			fmt.Fprintf(&sb, "    function create%s%s() {\n        const instance = {};\n        const internal = {};\n", iface.Name, root)
			internalEvents, err := m.internalEventsFor(iface, root)
			if err != nil {
				return "", err
			}
			for _, event := range internalEvents {
				if event.Impl.Kind != "Native" {
					return "", fmt.Errorf("internal event %s.%s must use a native implementation", iface.Name, event.Name)
				}
				argumentTypes := make([]string, 0, len(event.Params))
				for _, param := range event.Params {
					descriptor, err := jsTypeDescriptor(param.Type, m, root)
					if err != nil {
						return "", err
					}
					argumentTypes = append(argumentTypes, descriptor)
				}
				visibleRegistrationParams, err := m.parametersForProjection(event.RegistrationParams, iface.Tags, root)
				if err != nil {
					return "", err
				}
				registrationParams := make([]string, 0, len(visibleRegistrationParams))
				for _, param := range visibleRegistrationParams {
					descriptor, err := jsParameterDescriptor(param, m, root)
					if err != nil {
						return "", err
					}
					registrationParams = append(registrationParams, descriptor)
				}
				fmt.Fprintf(&sb, "        internal.%s = createListener(%s, %s, [%s], [%s]);\n", event.Name, strconv.Quote(iface.Name), strconv.Quote(event.Impl.Target), strings.Join(argumentTypes, ","), strings.Join(registrationParams, ","))
			}
			variants, err := m.variantsFor(iface, root)
			if err != nil {
				return "", err
			}
			groups := make(map[string][]functionDef)
			var order []string
			for _, fn := range variants {
				if _, ok := groups[fn.Name]; !ok {
					order = append(order, fn.Name)
				}
				groups[fn.Name] = append(groups[fn.Name], fn)
			}
			for _, name := range order {
				fmt.Fprintf(&sb, "        instance.%s = (...args) => {\n            const variants = [\n", name)
				for _, fn := range groups[name] {
					var params []string
					for _, param := range fn.Params {
						descriptor, err := jsParameterDescriptor(param, m, root)
						if err != nil {
							return "", err
						}
						params = append(params, descriptor)
					}
					// A callback accompanying a synchronous return value is part of
					// the API operation rather than an alternative async result. The
					// JavaScript implementation must receive it so it can preserve the
					// synchronous return value (for example contextMenus.create()).
					var cb *parameter
					if jsReturnKind(fn) != "sync" {
						cb = callbackParam(fn, m.Callbacks)
					}
					cbName := ""
					cbArity := 0
					var callbackResults []string
					if cb != nil {
						cbName = cb.Name
						callback := m.Callbacks[cb.Type.Name]
						cbArity = len(callback.Params)
						for _, callbackParam := range callback.Params {
							descriptor, err := jsTypeDescriptor(callbackParam.Type, m, root)
							if err != nil {
								return "", err
							}
							callbackResults = append(callbackResults, descriptor)
						}
					}
					resultDescriptor := "null"
					if resultType, ok := functionResultType(fn, m.Callbacks); ok {
						resultDescriptor, err = jsTypeDescriptor(resultType, m, root)
						if err != nil {
							return "", err
						}
					}
					fmt.Fprintf(&sb, "                {id:%s,binding:%s,implementation:%s,params:[%s],callback:%s,callbackArity:%d,callbackResults:[%s],returns:%s,result:%s},\n", strconv.Quote(fn.ID), strconv.Quote(fn.Impl.Target), strconv.Quote(fn.Impl.Kind), strings.Join(params, ","), strconv.Quote(cbName), cbArity, strings.Join(callbackResults, ","), strconv.Quote(jsReturnKind(fn)), resultDescriptor)
				}
				sb.WriteString(`            ];
            const matches = variants.map(variant => ({variant, match: __bridge_v2__.bind(args, variant.params)})).filter(item => item.match);
            matches.sort((a, b) => a.match.score - b.match.score);
            if(matches.length === 0 || (matches.length > 1 && matches[0].match.score === matches[1].match.score)) {
                throw new TypeError("No unambiguous v2 overload matches provided arguments");
            }
            const selected = matches[0];
            const nativeArgs = {...selected.match.values};
            const callback = selected.variant.callback ? nativeArgs[selected.variant.callback] : undefined;
            if(selected.variant.callback) delete nativeArgs[selected.variant.callback];
            const invoke = () => selected.variant.implementation === "JavaScript"
                ? __bridge_v2__.invokeImplementation(selected.variant.binding, {
                    id: selected.variant.id,
                    args: nativeArgs,
                    internal,
                    invokeNative: (binding, args = nativeArgs) => __bridge_v2__.postMessage(`)
				fmt.Fprintf(&sb, "%s, binding, args)\n", strconv.Quote(iface.Name))
				sb.WriteString(`                })
                : __bridge_v2__.postMessage(`)
				fmt.Fprintf(&sb, "%s, selected.variant.binding, nativeArgs);\n", strconv.Quote(iface.Name))
				sb.WriteString(`            if(callback) {
                let operation;
                try { operation = Promise.resolve(invoke()); }
                catch(error) { __bridge_v2__.callbackWithError(callback, error); return undefined; }
                operation.then(
                    value => {
                        if(selected.variant.callbackArity === 0) return callback();
                        if(selected.variant.callbackArity === 1) return callback(__bridge_v2__.project(value, selected.variant.callbackResults[0]));
                        const values = Array.isArray(value) ? value : [value];
                        return callback(...selected.variant.callbackResults.map((type, index) => __bridge_v2__.project(values[index], type)));
                    },
                    error => __bridge_v2__.callbackWithError(callback, error)
                );
                return undefined;
            }
            if(selected.variant.returns === "void") {
                try {
                    const operation = invoke();
                    if(operation && typeof operation.then === "function") operation.catch(error => console.error("WebExtensions v2 operation failed", error));
                } catch(error) {
                    console.error("WebExtensions v2 operation failed", error);
                }
                return undefined;
            }
            if(selected.variant.returns === "sync") {
                const value = invoke();
                return selected.variant.result ? __bridge_v2__.project(value, selected.variant.result) : value;
            }
            const operation = Promise.resolve(invoke());
            return selected.variant.result ? operation.then(value => __bridge_v2__.project(value, selected.variant.result)) : operation;
        };
`)
			}
			properties, err := m.propertiesFor(iface, root)
			if err != nil {
				return "", err
			}
			propertyNames := make(map[string]bool)
			for _, property := range properties {
				if propertyNames[property.Name] {
					continue
				}
				propertyNames[property.Name] = true
				if property.Impl.Kind != "JavaScript" {
					return "", fmt.Errorf("property %s.%s requires a JavaScript implementation", iface.Name, property.Name)
				}
				fmt.Fprintf(&sb, "        Object.defineProperty(instance, %s, {enumerable:true,configurable:false,get:() => __bridge_v2__.invokeImplementation(%s, {id:%s,args:{},internal})});\n", strconv.Quote(property.Name), strconv.Quote(property.Impl.Target), strconv.Quote(property.ID))
			}
			events, err := m.eventsFor(iface, root)
			if err != nil {
				return "", err
			}
			for _, event := range events {
				if event.Impl.Kind == "JavaScript" {
					fmt.Fprintf(&sb, "        instance.%s = __bridge_v2__.invokeImplementation(%s, {id:%s,args:{},internal,invokeNative:(binding,args={}) => __bridge_v2__.postMessage(%s,binding,args)});\n", event.Name, strconv.Quote(event.Impl.Target), strconv.Quote(event.ID), strconv.Quote(iface.Name))
					continue
				}
				argumentTypes := make([]string, 0, len(event.Params))
				for _, param := range event.Params {
					descriptor, err := jsTypeDescriptor(param.Type, m, root)
					if err != nil {
						return "", err
					}
					argumentTypes = append(argumentTypes, descriptor)
				}
				visibleRegistrationParams, err := m.parametersForProjection(event.RegistrationParams, iface.Tags, root)
				if err != nil {
					return "", err
				}
				registrationParams := make([]string, 0, len(visibleRegistrationParams))
				for _, param := range visibleRegistrationParams {
					descriptor, err := jsParameterDescriptor(param, m, root)
					if err != nil {
						return "", err
					}
					registrationParams = append(registrationParams, descriptor)
				}
				fmt.Fprintf(&sb, "        instance.%s = createListener(%s, %s, [%s], [%s]);\n", event.Name, strconv.Quote(iface.Name), strconv.Quote(event.Name), strings.Join(argumentTypes, ","), strings.Join(registrationParams, ","))
			}
			sb.WriteString("        return instance;\n    }\n\n")
		}
	}

	if includePreamble {
		sb.WriteString("    window.chrome = window.chrome || {};\n    window.browser = window.browser || {};\n")
	}
	for _, iface := range m.IDL.Interfaces {
		if iface.Global == "" || (selectedInterface != "" && iface.Name != selectedInterface) {
			continue
		}
		chromeFns, err := m.variantsFor(iface, "Chrome")
		if err != nil {
			return "", err
		}
		firefoxFns, err := m.variantsFor(iface, "Firefox")
		if err != nil {
			return "", err
		}
		chromeEvents, err := m.eventsFor(iface, "Chrome")
		if err != nil {
			return "", err
		}
		firefoxEvents, err := m.eventsFor(iface, "Firefox")
		if err != nil {
			return "", err
		}
		chromeProperties, err := m.propertiesFor(iface, "Chrome")
		if err != nil {
			return "", err
		}
		firefoxProperties, err := m.propertiesFor(iface, "Firefox")
		if err != nil {
			return "", err
		}
		chromeVisible := len(chromeFns)+len(chromeProperties)+len(chromeEvents) > 0
		firefoxVisible := len(firefoxFns)+len(firefoxProperties)+len(firefoxEvents) > 0
		shared, err := m.sharesRootObject(iface)
		if err != nil {
			return "", err
		}
		if shared {
			fmt.Fprintf(&sb, "    window.chrome.%s = window.browser.%s = create%s();\n", iface.Global, iface.Global, iface.Name)
		} else {
			if chromeVisible {
				fmt.Fprintf(&sb, "    window.chrome.%s = create%sChrome();\n", iface.Global, iface.Name)
			}
			if firefoxVisible {
				fmt.Fprintf(&sb, "    window.browser.%s = create%sFirefox();\n", iface.Global, iface.Name)
			}
		}
	}
	if includePostamble {
		sb.WriteString("    window.__bridge_v2__ = __bridge_v2__;\n    window.registerWebExtensionImplementation = (name, implementation) => __bridge_v2__.registerImplementation(name, implementation);\n})();\n")
	}
	return sb.String(), nil
}

func generateJS(m *model) (string, error) {
	return generateJSFragment(m, "", true, true)
}

func generateImpl(m *model, prefix string) (string, error) {
	bindings, err := m.nativeBindings()
	if err != nil {
		return "", err
	}
	var sb strings.Builder
	fmt.Fprintf(&sb, "#include \"%s_api.h\"\n#include <algorithm>\n#include <initializer_list>\n\n", prefix)
	sb.WriteString("namespace WebExtensions::v2 {\n\n")
	sb.WriteString(`namespace {
bool has_required_permissions(
    const std::vector<std::string>& permissions,
    std::initializer_list<const char*> required)
{
    for(const char* permission : required) {
        if(std::find(permissions.begin(), permissions.end(), permission) == permissions.end())
            return false;
    }
    return true;
}
} // namespace

`)
	for _, binding := range bindings {
		fmt.Fprintf(&sb, "struct %s_%s_ArgsStorage {\n", binding.Interface, safeIdent(binding.Target))
		for _, param := range binding.Params {
			storageType := m.cppStorageType(param.Type)
			if param.Optional {
				fmt.Fprintf(&sb, "    std::optional<%s> %s;\n", storageType, param.Name)
			} else {
				fmt.Fprintf(&sb, "    %s %s;\n", storageType, param.Name)
			}
		}
		fmt.Fprintln(&sb, "};")
		fmt.Fprintln(&sb)
	}
	sb.WriteString("bool native_call_bridge_received(int promise, const std::string& clazz, const std::string& function, const std::string& arguments_json, orion::bridge::WebViewBridge& webview, const std::string& extension_id, const std::string& script_world, SecurityProfile security_profile, const std::vector<std::string>& permissions, const Implementations& impls, std::string& error) {\n")
	for _, iface := range m.IDL.Interfaces {
		fmt.Fprintf(&sb, "    if(clazz == %s) {\n", strconv.Quote(iface.Name))
		fmt.Fprintf(&sb, "        if(!(%s)) { error = \"V2 API permission denied\"; return false; }\n", nativePermissionCondition(iface.Permissions))
		fmt.Fprintf(&sb, "        if(!impls.%s) { error = \"No v2 implementation for %s\"; return false; }\n", lowerFirst(iface.Name), iface.Name)
		fmt.Fprintf(&sb, "        %s& object = *impls.%s;\n", iface.Name, lowerFirst(iface.Name))
		for _, binding := range bindings {
			if binding.Interface != iface.Name {
				continue
			}
			argsType := binding.Interface + "_" + safeIdent(binding.Target) + "_ArgsStorage"
			fmt.Fprintf(&sb, "        if(function == %s) {\n", strconv.Quote(binding.Target))
			if len(binding.Permissions) > 0 {
				fmt.Fprintf(&sb, "            if(!(%s)) { error = \"V2 function permission denied\"; return false; }\n", nativePermissionCondition(binding.Permissions))
			}
			fmt.Fprintf(&sb, "            if(!(%s)) { error = \"V2 function is not available in this security profile\"; return false; }\n", securityProfileCondition(binding.Contexts))
			fmt.Fprintf(&sb, "            auto parsed = rfl::json::read<%s>(arguments_json);\n", argsType)
			sb.WriteString("            if(!parsed) { error = parsed.error().what(); return false; }\n")
			if len(binding.Params) > 0 {
				sb.WriteString("            auto args = std::move(*parsed);\n")
			}
			var callArgs []string
			for _, param := range binding.Params {
				if m.isDictionarySequence(param.Type) {
					viewName := param.Name + "_view"
					viewType := m.cppViewType(param.Type)
					if param.Optional {
						fmt.Fprintf(&sb, "            std::optional<%s> %s;\n", viewType, viewName)
						fmt.Fprintf(&sb, "            if(args.%s) {\n", param.Name)
						fmt.Fprintf(&sb, "                %s.emplace();\n", viewName)
						fmt.Fprintf(&sb, "                %s->reserve(args.%s->size());\n", viewName, param.Name)
						fmt.Fprintf(&sb, "                for(const auto& item : *args.%s) %s->push_back(from_storage(item));\n", param.Name, viewName)
						fmt.Fprintln(&sb, "            }")
						callArgs = append(callArgs, viewName+" ? &"+viewName+".value() : nullptr")
					} else {
						fmt.Fprintf(&sb, "            %s %s;\n", viewType, viewName)
						fmt.Fprintf(&sb, "            %s.reserve(args.%s.size());\n", viewName, param.Name)
						fmt.Fprintf(&sb, "            for(const auto& item : args.%s) %s.push_back(from_storage(item));\n", param.Name, viewName)
						callArgs = append(callArgs, viewName)
					}
				} else if _, named := m.Dictionaries[param.Type.Name]; named {
					if param.Optional {
						viewName := param.Name + "_view"
						fmt.Fprintf(&sb, "            std::optional<%s> %s;\n", m.cppViewType(param.Type), viewName)
						fmt.Fprintf(&sb, "            if(args.%s) %s.emplace(from_storage(*args.%s));\n", param.Name, viewName, param.Name)
						callArgs = append(callArgs, viewName+" ? &"+viewName+".value() : nullptr")
						continue
					}
					viewName := param.Name + "_view"
					fmt.Fprintf(&sb, "            auto %s = from_storage(args.%s);\n", viewName, param.Name)
					callArgs = append(callArgs, viewName)
				} else if param.Optional {
					callArgs = append(callArgs, "args."+param.Name+" ? &args."+param.Name+".value() : nullptr")
				} else {
					callArgs = append(callArgs, "args."+param.Name)
				}
			}
			fmt.Fprintf(&sb, "            object.%s(Promise<%s>{extension_id, webview, promise, script_world, security_profile}", binding.Method, m.cppViewType(binding.Return))
			for _, arg := range callArgs {
				fmt.Fprintf(&sb, ", %s", arg)
			}
			sb.WriteString(");\n            return true;\n        }\n")
		}
		sb.WriteString("        error = \"V2 function not found: \" + function; return false;\n    }\n")
	}
	sb.WriteString("    error = \"V2 class not found: \" + clazz; return false;\n}\n\n")
	sb.WriteString("bool native_event_bridge_received(int listener, const std::string& clazz, const std::string& event, const std::string& command, const std::string& registration_arguments_json, orion::bridge::WebViewBridge& webview, const std::string& extension_id, const std::string& script_world, SecurityProfile security_profile, const std::vector<std::string>& permissions, const Implementations& impls, std::string& error) {\n")
	for _, iface := range m.IDL.Interfaces {
		fmt.Fprintf(&sb, "    if(clazz == %s) {\n", strconv.Quote(iface.Name))
		fmt.Fprintf(&sb, "        if(!(%s)) { error = \"V2 API permission denied\"; return false; }\n", nativePermissionCondition(iface.Permissions))
		fmt.Fprintf(&sb, "        if(!impls.%s) { error = \"No v2 implementation for %s\"; return false; }\n", lowerFirst(iface.Name), iface.Name)
		for _, event := range iface.Events {
			if event.Impl.Kind != "Native" {
				continue
			}
			contexts, err := contextsFor(event.Tags, iface.Tags)
			if err != nil {
				return "", fmt.Errorf("event %s.%s: %w", iface.Name, event.Name, err)
			}
			fmt.Fprintf(&sb, "        if(event == %s) {\n", strconv.Quote(event.Name))
			fmt.Fprintf(&sb, "            if(!(%s)) { error = \"V2 event is not available in this security profile\"; return false; }\n", securityProfileCondition(contexts))
			fmt.Fprintf(&sb, "            if(command == \"add\") impls.%s->%s.addListener(webview, listener, registration_arguments_json, extension_id, script_world, security_profile);\n", lowerFirst(iface.Name), event.Name)
			fmt.Fprintf(&sb, "            else if(command == \"remove\") impls.%s->%s.removeListener(webview, listener, script_world);\n", lowerFirst(iface.Name), event.Name)
			sb.WriteString("            return true;\n        }\n")
		}
		sb.WriteString("        error = \"V2 event not found: \" + event; return false;\n    }\n")
	}
	sb.WriteString("    error = \"V2 class not found: \" + clazz; return false;\n}\n\n")
	preamble, err := generateJSFragment(m, "__no_interface__", true, false)
	if err != nil {
		return "", err
	}
	postamble, err := generateJSFragment(m, "__no_interface__", false, true)
	if err != nil {
		return "", err
	}
	sb.WriteString(genBridgeJsAppender(preamble, "Preamble"))

	type polyfillFragment struct {
		Name        string
		Profile     string
		Permissions []string
	}
	var fragments []polyfillFragment
	for _, profile := range securityProfiles {
		profileModel, err := newModel(m.IDL, profile.Tag)
		if err != nil {
			return "", err
		}
		for _, iface := range m.IDL.Interfaces {
			js, err := generateJSFragment(profileModel, iface.Name, false, false)
			if err != nil {
				return "", err
			}
			if strings.TrimSpace(js) == "" {
				continue
			}
			name := profile.Name + "_" + safeIdent(iface.Name)
			sb.WriteString(genBridgeJsAppender(js, name))
			fragments = append(fragments, polyfillFragment{
				Name: name, Profile: profile.Name, Permissions: iface.Permissions,
			})
		}
	}
	sb.WriteString(genBridgeJsAppender(postamble, "Postamble"))
	sb.WriteString(`std::string native_bridge_script(const PolyfillConfiguration& configuration) {
    std::string joined;
    append_bridge_v2_js_Preamble(joined);
    switch(configuration.security_profile) {
`)
	for _, profile := range securityProfiles {
		fmt.Fprintf(&sb, "        case SecurityProfile::%s:\n", profile.Name)
		for _, fragment := range fragments {
			if fragment.Profile != profile.Name {
				continue
			}
			condition := nativePermissionCondition(fragment.Permissions)
			if condition == "true" {
				fmt.Fprintf(&sb, "            append_bridge_v2_js_%s(joined);\n", fragment.Name)
			} else {
				condition = strings.Replace(
					condition, "(permissions,", "(configuration.permissions,", 1)
				fmt.Fprintf(&sb, "            if(%s) append_bridge_v2_js_%s(joined);\n", condition, fragment.Name)
			}
		}
		sb.WriteString("            break;\n")
	}
	sb.WriteString(`    }
    append_bridge_v2_js_Postamble(joined);
    return joined;
}
`)
	sb.WriteString("\n} // namespace WebExtensions::v2\n")
	return sb.String(), nil
}

// genBridgeJsAppender emits a source fragment as independent raw string
// literals of at most ~16 KB and a small function which appends them to the
// runtime-selected facade. MSVC
// rejects single string literals longer than 16380 bytes (C2026) and
// concatenated literals longer than 65535 bytes (C1091); the v2 bridge
// script exceeds both limits, so unlike generate.go's variant the chunks
// cannot simply be emitted as adjacent literals.
func genBridgeJsAppender(js, name string) string {
	var sb strings.Builder
	const chunkSize = 16000 // stay under MSVC's 16380-byte per-literal limit

	fmt.Fprintf(&sb, "static const char* const bridge_v2_js_parts_%s[] = {\n", name)
	for _, chunk := range splitStringChunks(js, chunkSize) {
		sb.WriteString("R\"v2js(")
		sb.WriteString(chunk)
		sb.WriteString(")v2js\",\n")
	}
	sb.WriteString("};\n\n")
	fmt.Fprintf(&sb, `static void append_bridge_v2_js_%s(std::string& joined) {
    for (const char* part : bridge_v2_js_parts_%s)
        joined += part;
}
`, name, name)
	return sb.String()
}

// splitStringChunks splits s into chunks of at most chunkSize bytes, never
// cutting a UTF-8 rune in half (copy of the helper in generate.go — the two
// generators are separate `go run` programs and cannot share code).
func splitStringChunks(s string, chunkSize int) []string {
	if chunkSize <= 0 || len(s) <= chunkSize {
		return []string{s}
	}

	chunks := make([]string, 0, (len(s)+chunkSize-1)/chunkSize)
	for len(s) > chunkSize {
		cut := chunkSize
		for cut > 0 && !utf8.RuneStart(s[cut]) {
			cut--
		}
		if cut == 0 {
			cut = chunkSize
		}
		chunks = append(chunks, s[:cut])
		s = s[cut:]
	}
	if len(s) > 0 {
		chunks = append(chunks, s)
	}
	return chunks
}

func main() {
	prefix := flag.String("prefix", "webkit_bridge_v2", "output filename prefix")
	visibility := flag.String("visibility", "", "pipe-separated visibility tags")
	flag.Parse()
	if flag.NArg() == 0 {
		fmt.Fprintln(os.Stderr, "Usage: go run generate_v2.go --prefix=<prefix> [--visibility=Tag|Tag] file.idlv2 ...")
		os.Exit(2)
	}
	idl := &idlFile{}
	for _, path := range flag.Args() {
		src, err := os.ReadFile(path)
		if err != nil {
			panic(err)
		}
		parsed, err := newParser(path, string(src)).parseFile()
		if err != nil {
			panic(err)
		}
		mergeIDL(idl, parsed)
	}
	m, err := newModel(idl, *visibility)
	if err != nil {
		panic(err)
	}
	header, err := generateHeader(m)
	if err != nil {
		panic(err)
	}
	impl, err := generateImpl(m, *prefix)
	if err != nil {
		panic(err)
	}
	outputs := map[string]string{
		*prefix + "_api.h":    header,
		*prefix + "_impl.cpp": impl,
	}
	keys := make([]string, 0, len(outputs))
	for path := range outputs {
		keys = append(keys, path)
	}
	sort.Strings(keys)
	for _, path := range keys {
		fmt.Println("Generating", path, "...")
		if err := os.WriteFile(path, []byte(outputs[path]), 0644); err != nil {
			panic(err)
		}
	}
}
