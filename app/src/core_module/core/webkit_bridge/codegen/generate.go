package main

import (
	"bufio"
	"flag"
	"fmt"
	"os"
	"regexp"
	"strconv"
	"strings"
	"unicode/utf8"
)

var (
	reMessage  = regexp.MustCompile(`^message\s+(\w+)`)
	reService  = regexp.MustCompile(`^service\s+(\w+)(.*)`)
	reField    = regexp.MustCompile(`^(repeatable\s+)?([\w<> ,]+)\s+(\w+);`)
	reRpc      = regexp.MustCompile(`^rpc\s+(\w+)\s*\(([^)]*)\)\s*returns\s*\(([^)]*)\)`)
	reListener = regexp.MustCompile(`^listener\s+(\w+)\s*\(([^)]*)\)\s*;`)
	reExtern   = regexp.MustCompile(`^extern\s+(\w+);`)
	reEnumDecl = regexp.MustCompile(`^(enum|object)\s+(\w+)(.*)`)
	reUsing    = regexp.MustCompile(`^using\s+(\w+)(?:\s*=\s*([\w\.]+))?;`)
)

var typeAliases = map[string]string{
	"json":   "string",
	"object": "any",
}

// ---- AST types ----
type Field struct {
	Name     string
	Type     string
	Repeated bool
}

type EnumValue struct {
	Name      string
	Literal   bool
	Value     string
	Reference string
}

type Enum struct {
	Name      string
	Values    []EnumValue
	Alias     string
	OwnerKind string
	OwnerName string
}

type Message struct {
	Name   string
	Fields []Field
	Enums  []*Enum
}

type Listener struct {
	Name   string
	Params []Field
}

type Rpc struct {
	Name        string
	Params      []Field
	Return      string
	BindingName string
}

type Service struct {
	Name      string
	Methods   []Rpc
	Listeners []Listener
	Fields    []Field // includes subservices
	Externs   []string
	Global    string
	Enums     []*Enum
}

type IDL struct {
	Messages []Message
	Services []Service
	Enums    []*Enum
}

// ---- Parser ----
func parseIDLFiles(paths []string) (*IDL, error) {
	var idl IDL
	for _, path := range paths {
		file, err := os.Open(path)
		if err != nil {
			return nil, err
		}
		defer file.Close()

		scanner := bufio.NewScanner(file)

		var currentMsg *Message
		var currentSvc *Service
		inBlock := ""

		type enumState struct {
			active    bool
			name      string
			ownerKind string
			ownerName string
			buffer    []string
		}
		var state enumState

		for scanner.Scan() {
			rawLine := scanner.Text()
			line := strings.TrimSpace(rawLine)
			if line == "" || strings.HasPrefix(line, "//") {
				continue
			}

			if state.active {
				if idx := strings.Index(line, "}"); idx != -1 {
					before := strings.TrimSpace(line[:idx])
					if before != "" {
						state.buffer = append(state.buffer, before)
					}
					values, err := parseEnumEntries(state.buffer)
					if err != nil {
						return nil, fmt.Errorf("parsing enum %s: %w", state.name, err)
					}

					enumDef := &Enum{
						Name:      state.name,
						Values:    values,
						OwnerKind: state.ownerKind,
						OwnerName: state.ownerName,
					}
					switch state.ownerKind {
					case "message":
						currentMsg.Enums = append(currentMsg.Enums, enumDef)
					case "service":
						currentSvc.Enums = append(currentSvc.Enums, enumDef)
					default:
						idl.Enums = append(idl.Enums, enumDef)
					}

					state = enumState{}
					continue
				}

				state.buffer = append(state.buffer, line)
				continue
			}

			switch {
			case reMessage.MatchString(line):
				m := reMessage.FindStringSubmatch(line)
				currentMsg = &Message{Name: m[1]}
				idl.Messages = append(idl.Messages, *currentMsg)
				inBlock = "message"

			case reService.MatchString(line):
				m := reService.FindStringSubmatch(line)
				rest := strings.TrimSpace(m[2])
				attrSection := rest
				if braceIdx := strings.Index(rest, "{"); braceIdx != -1 {
					attrSection = strings.TrimSpace(rest[:braceIdx])
				}
				global := parseServiceGlobal(attrSection)
				currentSvc = &Service{Name: m[1], Global: global}
				idl.Services = append(idl.Services, *currentSvc)
				inBlock = "service"

			case line == "}":
				if inBlock == "message" {
					idl.Messages[len(idl.Messages)-1] = *currentMsg
				} else if inBlock == "service" {
					idl.Services[len(idl.Services)-1] = *currentSvc
				}
				inBlock = ""

			default:
				if (inBlock == "message" || inBlock == "service" || inBlock == "") && reEnumDecl.MatchString(line) {
					m := reEnumDecl.FindStringSubmatch(line)
					name := m[2]
					rest := strings.TrimSpace(m[3])
					ownerKind := inBlock
					if ownerKind == "" {
						ownerKind = "global"
					}
					var ownerName string
					switch ownerKind {
					case "message":
						ownerName = currentMsg.Name
					case "service":
						ownerName = currentSvc.Name
					default:
						ownerName = ""
					}

					if strings.HasPrefix(rest, "=") {
						alias := normalizeEnumReference(strings.TrimPrefix(rest, "="))
						enumDef := &Enum{
							Name:      name,
							Alias:     alias,
							OwnerKind: ownerKind,
							OwnerName: ownerName,
						}
						switch ownerKind {
						case "message":
							currentMsg.Enums = append(currentMsg.Enums, enumDef)
						case "service":
							currentSvc.Enums = append(currentSvc.Enums, enumDef)
						default:
							idl.Enums = append(idl.Enums, enumDef)
						}
						continue
					}

					braceIdx := strings.Index(rest, "{")
					if braceIdx == -1 {
						return nil, fmt.Errorf("expected '{' in enum declaration for %s", name)
					}

					afterBrace := rest[braceIdx+1:]
					if idx := strings.Index(afterBrace, "}"); idx != -1 {
						body := strings.TrimSpace(afterBrace[:idx])
						var parts []string
						if body != "" {
							parts = append(parts, body)
						}
						values, err := parseEnumEntries(parts)
						if err != nil {
							return nil, fmt.Errorf("parsing enum %s: %w", name, err)
						}
						enumDef := &Enum{
							Name:      name,
							Values:    values,
							OwnerKind: ownerKind,
							OwnerName: ownerName,
						}
						switch ownerKind {
						case "message":
							currentMsg.Enums = append(currentMsg.Enums, enumDef)
						case "service":
							currentSvc.Enums = append(currentSvc.Enums, enumDef)
						default:
							idl.Enums = append(idl.Enums, enumDef)
						}
						continue
					}

					trimmed := strings.TrimSpace(afterBrace)
					state = enumState{
						active:    true,
						name:      name,
						ownerKind: ownerKind,
						ownerName: ownerName,
					}
					if trimmed != "" {
						state.buffer = append(state.buffer, trimmed)
					}
					continue
				}

				if reUsing.MatchString(line) {
					m := reUsing.FindStringSubmatch(line)
					name := m[1]
					target := m[2]
					if target == "" {
						target = name
					}
					ownerKind := inBlock
					if ownerKind == "" {
						ownerKind = "global"
					}
					var ownerName string
					switch ownerKind {
					case "message":
						ownerName = currentMsg.Name
					case "service":
						ownerName = currentSvc.Name
					default:
						ownerName = ""
					}
					enumDef := &Enum{
						Name:      name,
						Alias:     normalizeEnumReference(target),
						OwnerKind: ownerKind,
						OwnerName: ownerName,
					}
					switch ownerKind {
					case "message":
						currentMsg.Enums = append(currentMsg.Enums, enumDef)
					case "service":
						currentSvc.Enums = append(currentSvc.Enums, enumDef)
					default:
						idl.Enums = append(idl.Enums, enumDef)
					}
					continue
				}

				if inBlock == "message" && reField.MatchString(line) {
					f := reField.FindStringSubmatch(line)
					field := Field{
						Type:     canonicalizeTypeName(f[2]),
						Name:     f[3],
						Repeated: strings.TrimSpace(f[1]) != "",
					}
					currentMsg.Fields = append(currentMsg.Fields, field)
				}

				if inBlock == "service" {
					if reRpc.MatchString(line) {
						r := reRpc.FindStringSubmatch(line)
						params := parseParams(r[2])
						returnType := canonicalizeTypeName(r[3])
						rpc := Rpc{Name: r[1], Params: params, Return: returnType}
						currentSvc.Methods = append(currentSvc.Methods, rpc)

					} else if reListener.MatchString(line) {
						l := reListener.FindStringSubmatch(line)
						params := parseParams(l[2])
						listener := Listener{Name: l[1], Params: params}
						currentSvc.Listeners = append(currentSvc.Listeners, listener)

					} else if reExtern.MatchString(line) {
						e := reExtern.FindStringSubmatch(line)
						currentSvc.Externs = append(currentSvc.Externs, e[1])

					} else if reField.MatchString(line) {
						f := reField.FindStringSubmatch(line)
						field := Field{Type: canonicalizeTypeName(f[2]), Name: f[3], Repeated: strings.TrimSpace(f[1]) != ""}
						currentSvc.Fields = append(currentSvc.Fields, field)
					}
				}
			}
		}

		if state.active {
			return nil, fmt.Errorf("unterminated enum %s in %s", state.name, path)
		}

		if err := scanner.Err(); err != nil {
			return nil, err
		}
	}
	return &idl, nil
}

func assignRpcBindingNames(idl *IDL) {
	for i := range idl.Services {
		svc := &idl.Services[i]
		nameCounts := make(map[string]int)
		for _, rpc := range svc.Methods {
			nameCounts[rpc.Name]++
		}
		nameIndexes := make(map[string]int)
		for j := range svc.Methods {
			name := svc.Methods[j].Name
			if nameCounts[name] <= 1 {
				svc.Methods[j].BindingName = name
				continue
			}
			nameIndexes[name]++
			svc.Methods[j].BindingName = fmt.Sprintf("%s#%d", name, nameIndexes[name])
		}
	}
}

func buildMessageSet(idl *IDL) map[string]bool {
	messageSet := make(map[string]bool, len(idl.Messages))
	for _, msg := range idl.Messages {
		messageSet[msg.Name] = true
	}
	return messageSet
}

func jsParamTypeDescriptor(field Field, scopeOwner string, registry map[string]*Enum, messageSet map[string]bool) string {
	typeName := strings.TrimSpace(field.Type)
	if field.Repeated {
		return "array"
	}
	switch typeName {
	case "":
		return "any"
	case "bool":
		return "boolean"
	case "string":
		return "string"
	case "int32", "double":
		return "number"
	case "any":
		return "any"
	}

	if strings.HasPrefix(typeName, "map<") {
		return "object"
	}
	if messageSet[typeName] {
		return "object"
	}
	if isEnumTypeName(typeName, scopeOwner, registry) {
		return "string"
	}
	return "object"
}

func jsSignatureForRpc(rpc *Rpc, scopeOwner string, registry map[string]*Enum, messageSet map[string]bool) []string {
	signature := make([]string, 0, len(rpc.Params))
	for _, param := range rpc.Params {
		signature = append(signature, jsParamTypeDescriptor(param, scopeOwner, registry, messageSet))
	}
	return signature
}

func jsSignatureLiteral(signature []string) string {
	if len(signature) == 0 {
		return "[]"
	}
	parts := make([]string, len(signature))
	for i, item := range signature {
		parts[i] = strconv.Quote(item)
	}
	return "[" + strings.Join(parts, ", ") + "]"
}

func isAnyType(typeName string) bool {
	return strings.TrimSpace(typeName) == "any"
}

func isStringStringMap(typeName string) bool {
	t := strings.TrimSpace(typeName)
	if !strings.HasPrefix(t, "map<") || !strings.HasSuffix(t, ">") {
		return false
	}
	inner := strings.TrimSuffix(strings.TrimPrefix(t, "map<"), ">")
	parts := strings.Split(inner, ",")
	if len(parts) != 2 {
		return false
	}
	key := strings.TrimSpace(parts[0])
	val := strings.TrimSpace(parts[1])
	return key == "string" && val == "string"
}

func isStringAnyMap(typeName string) bool {
	t := strings.TrimSpace(typeName)
	if !strings.HasPrefix(t, "map<") || !strings.HasSuffix(t, ">") {
		return false
	}
	inner := strings.TrimSuffix(strings.TrimPrefix(t, "map<"), ">")
	parts := splitParamsTopLevel(inner)
	if len(parts) != 2 {
		return false
	}
	key := strings.TrimSpace(parts[0])
	val := strings.TrimSpace(parts[1])
	return key == "string" && val == "any"
}

func canonicalizeTypeName(typeName string) string {
	original := typeName
	trimmed := strings.TrimSpace(typeName)

	if mapped, ok := typeAliases[trimmed]; ok {
		return mapped
	}

	if strings.HasPrefix(trimmed, "map<") && strings.HasSuffix(trimmed, ">") {
		inner := strings.TrimPrefix(trimmed, "map<")
		inner = strings.TrimSuffix(inner, ">")
		parts := splitParamsTopLevel(inner)
		if len(parts) == 2 {
			key := canonicalizeTypeName(parts[0])
			val := canonicalizeTypeName(parts[1])
			if key == parts[0] && val == parts[1] {
				return original
			}
			return fmt.Sprintf("map<%s, %s>", key, val)
		}
	}

	return original
}

func parseParams(paramStr string) []Field {
	paramStr = strings.TrimSpace(paramStr)
	if paramStr == "" {
		return nil
	}
	parts := splitParamsTopLevel(paramStr)
	var fields []Field
	for _, p := range parts {
		p = strings.TrimSpace(p)
		if p == "" {
			continue
		}
		repeated := false
		if strings.HasPrefix(p, "repeatable ") {
			repeated = true
			p = strings.TrimSpace(strings.TrimPrefix(p, "repeatable "))
		}
		f := strings.Fields(p)
		if len(f) < 2 {
			continue
		}
		typeName := canonicalizeTypeName(f[0])
		name := f[1]
		fields = append(fields, Field{Type: typeName, Name: name, Repeated: repeated})
	}
	return fields
}

func parseServiceGlobal(attrStr string) string {
	attrStr = strings.TrimSpace(attrStr)
	if attrStr == "" {
		return ""
	}
	if strings.HasPrefix(attrStr, "[") && strings.HasSuffix(attrStr, "]") {
		attrStr = strings.TrimSpace(attrStr[1 : len(attrStr)-1])
	}
	if attrStr == "" {
		return ""
	}
	parts := strings.Split(attrStr, ",")
	for _, part := range parts {
		item := strings.TrimSpace(part)
		if strings.HasPrefix(item, "global") {
			kv := strings.SplitN(item, "=", 2)
			if len(kv) == 2 {
				val := strings.TrimSpace(kv[1])
				if after, ok := strings.CutPrefix(val, "browser."); ok {
					val = after
				}
				val = strings.TrimPrefix(val, ".")
				return val
			}
		}
	}
	return ""
}

func normalizeEnumReference(ref string) string {
	ref = strings.TrimSpace(ref)
	ref = strings.TrimSuffix(ref, ";")
	ref = strings.TrimSuffix(ref, ",")
	ref = strings.ReplaceAll(ref, "::", ".")
	ref = strings.ReplaceAll(ref, " ", "")
	return ref
}

func splitEnumEntries(body string) []string {
	var entries []string
	var current strings.Builder
	depth := 0
	inString := false
	var stringChar rune

	for _, r := range body {
		switch r {
		case '"', '\'':
			if !inString {
				inString = true
				stringChar = r
			} else if stringChar == r {
				inString = false
			}
			current.WriteRune(r)
		case '{', '[':
			if !inString {
				depth++
			}
			current.WriteRune(r)
		case '}', ']':
			if !inString && depth > 0 {
				depth--
			}
			current.WriteRune(r)
		case ',', ';':
			if !inString && depth == 0 {
				entry := strings.TrimSpace(current.String())
				if entry != "" {
					entries = append(entries, entry)
				}
				current.Reset()
			} else {
				current.WriteRune(r)
			}
		default:
			current.WriteRune(r)
		}
	}

	if tail := strings.TrimSpace(current.String()); tail != "" {
		entries = append(entries, tail)
	}
	return entries
}

func splitParamsTopLevel(paramStr string) []string {
	var parts []string
	var current strings.Builder
	depth := 0
	for _, r := range paramStr {
		switch r {
		case '<':
			depth++
			current.WriteRune(r)
		case '>':
			if depth > 0 {
				depth--
			}
			current.WriteRune(r)
		case ',':
			if depth == 0 {
				token := strings.TrimSpace(current.String())
				if token != "" {
					parts = append(parts, token)
				}
				current.Reset()
			} else {
				current.WriteRune(r)
			}
		default:
			current.WriteRune(r)
		}
	}
	if tail := strings.TrimSpace(current.String()); tail != "" {
		parts = append(parts, tail)
	}
	return parts
}

func parseEnumEntries(lines []string) ([]EnumValue, error) {
	body := strings.TrimSpace(strings.Join(lines, " "))
	if body == "" {
		return nil, nil
	}

	rawEntries := splitEnumEntries(body)
	values := make([]EnumValue, 0, len(rawEntries))
	for _, entry := range rawEntries {
		parts := strings.SplitN(entry, ":", 2)
		if len(parts) != 2 {
			parts = strings.SplitN(entry, "=", 2)
			if len(parts) != 2 {
				return nil, fmt.Errorf("invalid enum entry %q", entry)
			}
		}
		name := strings.TrimSpace(parts[0])
		if name == "" {
			return nil, fmt.Errorf("enum entry missing name in %q", entry)
		}
		valueStr := strings.TrimSpace(parts[1])
		valueStr = strings.TrimSuffix(valueStr, ",")
		valueStr = strings.TrimSuffix(valueStr, ";")
		valueStr = strings.TrimSpace(valueStr)
		valueStr = strings.TrimSpace(valueStr)

		ev := EnumValue{Name: name}
		if len(valueStr) > 1 && ((valueStr[0] == '"' && valueStr[len(valueStr)-1] == '"') || (valueStr[0] == '\'' && valueStr[len(valueStr)-1] == '\'')) {
			ev.Literal = true
			ev.Value = strings.Trim(valueStr, "\"'")
		} else if valueStr != "" {
			ev.Reference = normalizeEnumReference(valueStr)
		} else {
			return nil, fmt.Errorf("enum entry %s has empty value", name)
		}
		values = append(values, ev)
	}

	return values, nil
}

type ResolvedEnumValue struct {
	Name  string
	Value string
}

func enumQualifiedName(e *Enum) string {
	if e.OwnerName == "" {
		return e.Name
	}
	return e.OwnerName + "." + e.Name
}

func buildEnumRegistry(idl *IDL) map[string]*Enum {
	registry := make(map[string]*Enum)
	for _, e := range idl.Enums {
		if e.OwnerKind == "" {
			e.OwnerKind = "global"
		}
		registry[enumQualifiedName(e)] = e
	}
	for i := range idl.Messages {
		msg := &idl.Messages[i]
		for _, e := range msg.Enums {
			if e.OwnerKind == "" {
				e.OwnerKind = "message"
			}
			if e.OwnerName == "" {
				e.OwnerName = msg.Name
			}
			registry[enumQualifiedName(e)] = e
		}
	}

	for i := range idl.Services {
		svc := &idl.Services[i]
		for _, e := range svc.Enums {
			if e.OwnerKind == "" {
				e.OwnerKind = "service"
			}
			if e.OwnerName == "" {
				e.OwnerName = svc.Name
			}
			registry[enumQualifiedName(e)] = e
		}
	}
	return registry
}

func resolveEnumReference(ref string, context *Enum, registry map[string]*Enum) (*Enum, error) {
	ref = normalizeEnumReference(ref)
	if ref == "" {
		return nil, fmt.Errorf("empty enum reference")
	}
	if e, ok := registry[ref]; ok {
		return e, nil
	}
	if context != nil && context.OwnerName != "" {
		if e, ok := registry[context.OwnerName+"."+ref]; ok {
			return e, nil
		}
	}
	return nil, fmt.Errorf("unknown enum reference %q", ref)
}

func splitEnumValueReference(ref string, context *Enum) (string, string, error) {
	ref = normalizeEnumReference(ref)
	if ref == "" {
		return "", "", fmt.Errorf("empty enum value reference")
	}
	parts := strings.Split(ref, ".")
	if len(parts) == 1 {
		if context == nil {
			return "", "", fmt.Errorf("cannot resolve enum value reference %q without context", ref)
		}
		return context.OwnerName + "." + context.Name, parts[0], nil
	}
	if len(parts) < 2 {
		return "", "", fmt.Errorf("invalid enum value reference %q", ref)
	}
	enumName := strings.Join(parts[:len(parts)-1], ".")
	constant := parts[len(parts)-1]
	return enumName, constant, nil
}

func resolveEnumValuesForJS(enum *Enum, registry map[string]*Enum, visited map[string]bool) ([]ResolvedEnumValue, error) {
	if visited == nil {
		visited = make(map[string]bool)
	}

	key := enumQualifiedName(enum)
	if visited[key] {
		return nil, fmt.Errorf("cyclic enum reference involving %s", key)
	}
	visited[key] = true
	defer delete(visited, key)

	if enum.Alias != "" {
		target, err := resolveEnumReference(enum.Alias, enum, registry)
		if err != nil {
			return nil, err
		}
		return resolveEnumValuesForJS(target, registry, visited)
	}

	results := make([]ResolvedEnumValue, 0, len(enum.Values))
	for _, val := range enum.Values {
		if val.Literal {
			results = append(results, ResolvedEnumValue{Name: val.Name, Value: val.Value})
			continue
		}
		enumName, constantName, err := splitEnumValueReference(val.Reference, enum)
		if err != nil {
			return nil, err
		}
		target, err := resolveEnumReference(enumName, enum, registry)
		if err != nil {
			return nil, err
		}
		resolved, err := resolveEnumValuesForJS(target, registry, visited)
		if err != nil {
			return nil, err
		}
		found := false
		for _, candidate := range resolved {
			if candidate.Name == constantName {
				results = append(results, ResolvedEnumValue{Name: val.Name, Value: candidate.Value})
				found = true
				break
			}
		}
		if !found {
			return nil, fmt.Errorf("enum constant %s not found in %s", constantName, enumQualifiedName(target))
		}
	}

	return results, nil
}

// ---- Debug printer ----
func printIDL(idl *IDL) {
	fmt.Printf("Parsed %d messages, %d services\n", len(idl.Messages), len(idl.Services))

	for _, m := range idl.Messages {
		fmt.Println("Message:", m.Name)
		for _, f := range m.Fields {
			rep := ""
			if f.Repeated {
				rep = "repeatable "
			}
			fmt.Printf("  %s%s %s\n", rep, f.Type, f.Name)
		}
	}

	for _, s := range idl.Services {
		fmt.Println("Service:", s.Name, "global?", s.Global)
		for _, rpc := range s.Methods {
			fmt.Printf("  RPC %s -> %s\n", rpc.Name, rpc.Return)
			for _, p := range rpc.Params {
				fmt.Printf("    param %s %s\n", p.Type, p.Name)
			}
		}
		for _, l := range s.Listeners {
			var parts []string
			for _, p := range l.Params {
				part := fmt.Sprintf("%s %s", p.Type, p.Name)
				parts = append(parts, part)
			}
			fmt.Printf("  Listener %s(%s)\n", l.Name, strings.Join(parts, ", "))
		}
		for _, f := range s.Fields {
			rep := ""
			if f.Repeated {
				rep = "repeatable "
			}
			fmt.Printf("  Field: %s%s %s\n", rep, f.Type, f.Name)
		}
	}
}

func cppType(t string, repeated bool, scopeOwner string, registry map[string]*Enum) string {
	base := t
	switch t {
	case "bool":
		base = "bool"
	case "string":
		base = "std::string"
	case "any":
		base = "rfl::Generic"
	case "int32":
		base = "int"
	case "double":
		base = "double"
	}

	// map<K,V>
	if strings.HasPrefix(t, "map<") {
		inner := strings.TrimPrefix(t, "map<")
		inner = strings.TrimSuffix(inner, ">")
		parts := strings.Split(inner, ",")
		if len(parts) == 2 {
			k := strings.TrimSpace(parts[0])
			v := strings.TrimSpace(parts[1])
			keyType := cppType(k, false, scopeOwner, registry)
			valType := cppType(v, false, scopeOwner, registry)
			base = fmt.Sprintf("std::map<%s, %s>", keyType, valType)
		}
	}

	if isEnumTypeName(t, scopeOwner, registry) {
		base = "std::string"
	}

	if repeated {
		return "std::vector<" + base + ">"
	}
	return base
}

func isEnumTypeName(typeName string, scopeOwner string, registry map[string]*Enum) bool {
	normalized := normalizeEnumReference(typeName)
	if _, ok := registry[normalized]; ok {
		return true
	}
	if scopeOwner != "" {
		if _, ok := registry[scopeOwner+"."+normalized]; ok {
			return true
		}
	}
	return false
}

func toCppQualifiedName(name string) string {
	return strings.ReplaceAll(name, ".", "::")
}

func cppEnumValueExpression(val EnumValue) string {
	if val.Literal {
		return fmt.Sprintf("%q", val.Value)
	}
	if val.Reference != "" {
		return toCppQualifiedName(val.Reference)
	}
	return "\"\""
}

func renderCppEnum(sb *strings.Builder, indent string, enumDef *Enum, registry map[string]*Enum) {
	if enumDef.Alias != "" {
		target, err := resolveEnumReference(enumDef.Alias, enumDef, registry)
		if err != nil {
			panic(err)
		}
		qualified := toCppQualifiedName(enumQualifiedName(target))
		if qualified != "" {
			qualified = "::WebExtensions::" + qualified
		}
		fmt.Fprintf(sb, "%susing %s = %s;\n", indent, enumDef.Name, qualified)
		return
	}

	fmt.Fprintf(sb, "%sstruct %s {\n", indent, enumDef.Name)
	for _, val := range enumDef.Values {
		fmt.Fprintf(sb, "%s    static constexpr const char* %s = %s;\n", indent, val.Name, cppEnumValueExpression(val))
	}
	fmt.Fprintf(sb, "%s};\n", indent)
}

func genApiHeader(idl *IDL) string {
	var sb strings.Builder
	registry := buildEnumRegistry(idl)

	sb.WriteString(
		`#pragma once

#include <string>
#include <optional>
#include <vector>
#include <map>
#include <tuple>
#include <utility>
#include <rfl/json.hpp>
#include "core/webkit_bridge/API/bridge.h"

namespace WebExtensions {

`)

	// ---- messages forward declarations ----
	for _, m := range idl.Messages {
		fmt.Fprintf(&sb, "struct %s;\n", m.Name)
	}

	sb.WriteString("\n")

	for _, m := range idl.Services {
		fmt.Fprintf(&sb, "class %s;\n", m.Name)
	}

	sb.WriteString("\nstruct Implementations {\n")
	for _, m := range idl.Services {
		fmt.Fprintf(&sb, "    %s* %s;\n", m.Name, strings.ToLower(m.Name))
	}
	sb.WriteString("};\n")
	sb.WriteString("\n")
	sb.WriteString("template<typename T>\n")
	sb.WriteString("Implementations make_implementations(T* instance) {\n")
	sb.WriteString("    Implementations impls{};\n")
	for _, m := range idl.Services {
		fmt.Fprintf(&sb, "    if constexpr (std::is_base_of_v<%s, T>) { impls.%s = static_cast<%s*>(instance); }\n", m.Name, strings.ToLower(m.Name), m.Name)
	}
	sb.WriteString("    return impls;\n")
	sb.WriteString("}\n")

	sb.WriteString(
		`
bool native_call_bridge_received(int promise, const std::string& clazz, const std::string& function, const std::string& arguments_json, orion::bridge::WebViewBridge& webview, const std::string& extension_id, const std::string& script_world, const Implementations& impls, std::string& error);
bool native_event_bridge_received(int listener, const std::string& clazz, const std::string& event, const std::string& command, orion::bridge::WebViewBridge& webview, const std::string& extension_id, const std::string& script_world, const Implementations& impls, std::string& error);

void native_call_not_implemented(const char* call);
const char* native_bridge_script();
`)

	if len(idl.Enums) > 0 {
		sb.WriteString("\n")
		for _, enumDef := range idl.Enums {
			renderCppEnum(&sb, "", enumDef, registry)
		}
	}

	sb.WriteString(
		`
template<typename T>
std::optional<T> deserialize_json(const std::string& json, std::string* error = nullptr) {
    auto result = rfl::json::read<T>(json);
    if(!result) {
        if(error) {
            *error = result.error().what();
        }
        return std::nullopt;
    }
    return std::optional<T>(std::move(*result));
}`)

	sb.WriteString("\n\n")

	// ---- messages ----
	for _, m := range idl.Messages {
		fmt.Fprintf(&sb, "struct %s {\n", m.Name)
		if len(m.Enums) > 0 {
			for _, enumDef := range m.Enums {
				renderCppEnum(&sb, "    ", enumDef, registry)
				sb.WriteString("\n")
			}
		}

		for _, f := range m.Fields {
			ctype := cppType(f.Type, f.Repeated, m.Name, registry)
			fmt.Fprintf(&sb, "    std::optional<%s> %s;\n", ctype, f.Name)
		}
		sb.WriteString("};\n\n")
	}

	sb.WriteString(
		`template<typename... Args>
class Promise
{
public:
    Promise(orion::bridge::WebViewBridge& webview, int promise, std::string script_world = {}) : m_webview(webview.weak_from_this()), m_promise(promise), m_script_world(std::move(script_world)) {};
    std::weak_ptr<orion::bridge::WebViewBridge> webview() const { return m_webview; }
    void resolve(const std::optional<Args>&... args) const {
        handle_promise(true, args...);
    }
    void reject() const {
        handle_reject();
    }
    // A single string argument is turned into an Error by the JS side.
    void reject(const std::string& message) const {
        handle_reject_message(message);
    }
private:
    void send_payload(const std::string& payload_json) const {
        if(auto webview = m_webview.lock()) webview->web_view_call("__bridge__.handlePromise(...arguments)", payload_json, m_script_world);
    }
    void handle_promise(bool resolve, const std::optional<Args>&... args) const {
        struct Payload {
            int promise;
            bool resolve;
            std::tuple<std::optional<Args>...> arguments;
        };
        Payload payload{m_promise, resolve, std::make_tuple(args...)};
        send_payload(rfl::json::write(payload));
    }
    void handle_reject() const {
        struct Payload {
            int promise;
            bool resolve;
            std::tuple<> arguments;
        };
        Payload payload{m_promise, false, {}};
        send_payload(rfl::json::write(payload));
    }
    void handle_reject_message(const std::string& message) const {
        struct Payload {
            int promise;
            bool resolve;
            std::tuple<std::string> arguments;
        };
        Payload payload{m_promise, false, {message}};
        send_payload(rfl::json::write(payload));
    }
private:
    std::weak_ptr<orion::bridge::WebViewBridge> m_webview;
    int m_promise;
    std::string m_script_world;
};

template<>
class Promise<void>
{
public:
    Promise(orion::bridge::WebViewBridge& webview, int promise, std::string script_world = {}) : m_webview(webview.weak_from_this()), m_promise(promise), m_script_world(std::move(script_world)) {};
    std::weak_ptr<orion::bridge::WebViewBridge> webview() const { return m_webview; }
    void resolve() const { handle_promise(true); }
    void reject() const { handle_promise(false); }
    // A single string argument is turned into an Error by the JS side.
    void reject(const std::string& message) const {
        struct Payload {
            int promise;
            bool resolve;
            std::tuple<std::string> arguments;
        };
        Payload payload{m_promise, false, {message}};
        send_payload(rfl::json::write(payload));
    }
private:
    void send_payload(const std::string& payload_json) const {
        if(auto webview = m_webview.lock()) webview->web_view_call("__bridge__.handlePromise(...arguments)", payload_json, m_script_world);
    }
    void handle_promise(bool resolve) const {
        struct Payload {
            int promise;
            bool resolve;
            std::tuple<> arguments;
        };
        Payload payload{m_promise, resolve, {}};
        send_payload(rfl::json::write(payload));
    }
private:
    std::weak_ptr<orion::bridge::WebViewBridge> m_webview;
    int m_promise;
    std::string m_script_world;
};

`)

	sb.WriteString(
		`template<typename... Args>
class Listener
{
public:
    void addListener(orion::bridge::WebViewBridge& webview, int listener, std::string script_world = {}) {
        m_listeners[webview.weak_from_this()].insert_or_assign(listener, std::move(script_world));
    }
    void removeListener(orion::bridge::WebViewBridge& webview, int listener) {
        auto it = m_listeners.find(webview.weak_from_this());
        if(it == m_listeners.end()) return;
        it->second.erase(listener);
        if(it->second.empty()) m_listeners.erase(it);
    }
    void notify_one(orion::bridge::WebViewBridge& webview, int listener, const std::string& script_world, Args... args) {
        struct Payload {
            int listener;
            std::tuple<Args...> arguments;
        };
        Payload payload{listener, std::make_tuple(args...)};
        const std::string payload_json = rfl::json::write(payload);
        webview.web_view_call("__bridge__.handleNotify(...arguments)", payload_json, script_world);
    }
    void notify_all(Args... args) {
        for(auto webview_it = m_listeners.begin(); webview_it != m_listeners.end();) {
            auto webview = webview_it->first.lock();
            if(!webview) {
                webview_it = m_listeners.erase(webview_it);
                continue;
            }
            for(const auto& listener : webview_it->second) {
                notify_one(*webview, listener.first, listener.second, args...);
            }
            ++webview_it;
        }
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
    size_t notify_if(const Predicate& predicate, Args... args) {
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
            for(const auto& listener : webview_it->second) {
                notify_one(*webview, listener.first, listener.second, args...);
                ++notified;
            }
            ++webview_it;
        }
        return notified;
    }
    void notify(Args... args) {
        notify_all(args...);
    }
    void operator()(Args... args) {
        notify(args...);
    }
private:
    using WebViewRef = std::weak_ptr<orion::bridge::WebViewBridge>;
    std::map<WebViewRef, std::map<int, std::string>, std::owner_less<WebViewRef>> m_listeners;
};

`)

	// ---- services ----
	for _, s := range idl.Services {
		fmt.Fprintf(&sb, "class %s {\npublic:\n", s.Name)

		if len(s.Enums) > 0 {
			for _, enumDef := range s.Enums {
				renderCppEnum(&sb, "    ", enumDef, registry)
			}
		}

		// RPC methods
		for _, rpc := range s.Methods {
			rettype := rpc.Return
			promiseType := "void"
			repeated := false

			if rettype != "" && rettype != "()" {
				if strings.HasPrefix(rettype, "repeatable ") {
					repeated = true
					rettype = strings.TrimPrefix(rettype, "repeatable ")
				}
				promiseType = cppType(rettype, repeated, s.Name, registry)
			}
			fmt.Fprintf(&sb, "    virtual void %s(const std::string& extension_id, Promise<%s> promise",
				rpc.Name, promiseType)

			// parameters
			for _, p := range rpc.Params {
				fmt.Fprintf(&sb, ", const std::optional<%s>& %s", cppType(p.Type, p.Repeated, s.Name, registry), p.Name)
			}

			fmt.Fprintf(&sb, ") { native_call_not_implemented(\"%s::%s\"); promise.reject(\"%s.%s is not implemented\"); };\n", s.Name, rpc.BindingName, s.Name, rpc.Name)
		}

		// listeners
		for _, l := range s.Listeners {
			paramTypes := make([]string, 0, len(l.Params))
			for _, p := range l.Params {
				paramTypes = append(paramTypes, cppType(p.Type, p.Repeated, s.Name, registry))
			}
			listenerType := "Listener"
			if len(paramTypes) > 0 {
				listenerType += "<" + strings.Join(paramTypes, ", ") + ">"
			} else {
				listenerType += "<>"
			}
			fmt.Fprintf(&sb, "    %s %s;\n", listenerType, l.Name)
		}

		// subservices
		for _, f := range s.Fields {
			fmt.Fprintf(&sb, "    virtual %s& get%s() = 0;\n", f.Type, strings.Title(f.Name))
		}

		sb.WriteString("};\n\n")
	}

	sb.WriteString("} // namespace WebExtensions\n")
	return sb.String()
}

func genBridgeJs(idl *IDL) string {
	var sb strings.Builder
	registry := buildEnumRegistry(idl)
	messageSet := buildMessageSet(idl)

	sb.WriteString("(function () {\n")
	sb.WriteString("    const __bridge__ = {\n")
	sb.WriteString("        nativeCallHandler: window.__webextensions_native_call_handler__ || \"nativeCallBridge\",\n")
	sb.WriteString("        nativeEventHandler: window.__webextensions_native_event_handler__ || \"nativeEventBridge\",\n")
	sb.WriteString("        promiseRegistry: {},\n")
	sb.WriteString("        nextPromiseId: 1,\n")
	sb.WriteString("        postMessage(className, fn, ...args) {\n")
	sb.WriteString("            return new Promise((resolve_cb, reject_cb) => {\n")
	sb.WriteString("                const promiseId = this.nextPromiseId++;\n")
	sb.WriteString("                this.promiseRegistry[promiseId] = { resolve: resolve_cb, reject: reject_cb };\n")
	sb.WriteString("                let argsJson = \"[]\";\n")
	sb.WriteString("                try {\n")
	sb.WriteString("                    argsJson = JSON.stringify(args);\n")
	sb.WriteString("                } catch (e) {\n")
	sb.WriteString("                    delete this.promiseRegistry[promiseId];\n")
	sb.WriteString("                    reject_cb(e);\n")
	sb.WriteString("                    return;\n")
	sb.WriteString("                }\n")
	sb.WriteString("                const payload = { class: className, function: fn, arguments: argsJson, promise: promiseId, };\n")
	sb.WriteString("                window.webkit.messageHandlers[this.nativeCallHandler].postMessage(JSON.stringify(payload));\n")
	sb.WriteString("            });\n")
	sb.WriteString("        },\n")
	sb.WriteString("        parseJson(value) {\n")
	sb.WriteString("            if (typeof value !== \"string\") {\n")
	sb.WriteString("                return value;\n")
	sb.WriteString("            }\n")
	sb.WriteString("            try {\n")
	sb.WriteString("                return JSON.parse(value);\n")
	sb.WriteString("            } catch (e) {\n")
	sb.WriteString("                console.error(\"WebExtensions: failed to parse JSON message\", e);\n")
	sb.WriteString("                return null;\n")
	sb.WriteString("            }\n")
	sb.WriteString("        },\n")
	sb.WriteString("        encodeAny(value) {\n")
	sb.WriteString("            try {\n")
	sb.WriteString("                return JSON.stringify(value);\n")
	sb.WriteString("            } catch (e) {\n")
	sb.WriteString("                return null;\n")
	sb.WriteString("            }\n")
	sb.WriteString("        },\n")
	sb.WriteString("        decodeAny(value) {\n")
	sb.WriteString("            try {\n")
	sb.WriteString("                return JSON.parse(value);\n")
	sb.WriteString("            } catch (e) {\n")
	sb.WriteString("                return value;\n")
	sb.WriteString("            }\n")
	sb.WriteString("        },\n")
	sb.WriteString("        decodeAnyMap(value) {\n")
	sb.WriteString("            if (value === undefined) {\n")
	sb.WriteString("                return undefined;\n")
	sb.WriteString("            }\n")
	sb.WriteString("            if (value === null) {\n")
	sb.WriteString("                return null;\n")
	sb.WriteString("            }\n")
	sb.WriteString("            if (typeof value !== \"object\") {\n")
	sb.WriteString("                return value;\n")
	sb.WriteString("            }\n")
	sb.WriteString("            return Object.fromEntries(Object.entries(value).map(([k, v]) => [k, this.decodeAny(v)]));\n")
	sb.WriteString("        },\n")
	sb.WriteString("        encodeStringMap(value) {\n")
	sb.WriteString("            if (value === undefined) {\n")
	sb.WriteString("                return undefined;\n")
	sb.WriteString("            }\n")
	sb.WriteString("            if (value === null) {\n")
	sb.WriteString("                return null;\n")
	sb.WriteString("            }\n")
	sb.WriteString("            if (!value || typeof value !== \"object\") {\n")
	sb.WriteString("                return {};\n")
	sb.WriteString("            }\n")
	sb.WriteString("            return Object.fromEntries(Object.entries(value).map(([k, v]) => [k, JSON.stringify(v)]));\n")
	sb.WriteString("        },\n")
	sb.WriteString("        matchSignature(args, signature) {\n")
	sb.WriteString("            if (args.length !== signature.length) {\n")
	sb.WriteString("                return false;\n")
	sb.WriteString("            }\n")
	sb.WriteString("            for (let i = 0; i < signature.length; ++i) {\n")
	sb.WriteString("                if (!this.typeMatches(args[i], signature[i])) {\n")
	sb.WriteString("                    return false;\n")
	sb.WriteString("                }\n")
	sb.WriteString("            }\n")
	sb.WriteString("            return true;\n")
	sb.WriteString("        },\n")
	sb.WriteString("        typeMatches(value, expected) {\n")
	sb.WriteString("            if (!expected || expected === \"any\") {\n")
	sb.WriteString("                return true;\n")
	sb.WriteString("            }\n")
	sb.WriteString("            if (value === undefined || value === null) {\n")
	sb.WriteString("                return true;\n")
	sb.WriteString("            }\n")
	sb.WriteString("            switch (expected) {\n")
	sb.WriteString("                case \"string\":\n")
	sb.WriteString("                    return typeof value === \"string\";\n")
	sb.WriteString("                case \"boolean\":\n")
	sb.WriteString("                    return typeof value === \"boolean\";\n")
	sb.WriteString("                case \"number\":\n")
	sb.WriteString("                    return typeof value === \"number\";\n")
	sb.WriteString("                case \"array\":\n")
	sb.WriteString("                    return Array.isArray(value);\n")
	sb.WriteString("                case \"object\":\n")
	sb.WriteString("                    return value === null || (typeof value === \"object\" && !Array.isArray(value));\n")
	sb.WriteString("                case \"function\":\n")
	sb.WriteString("                    return typeof value === \"function\";\n")
	sb.WriteString("            }\n")
	sb.WriteString("            return true;\n")
	sb.WriteString("        },\n")
	sb.WriteString("        handlePromise(...args) {\n")
	sb.WriteString("            const msg = this.parseJson(args[0]);\n")
	sb.WriteString("            if (!msg) {\n")
	sb.WriteString("                return;\n")
	sb.WriteString("            }\n")
	sb.WriteString("            const promise = this.promiseRegistry[msg.promise];\n")
	sb.WriteString("            const call = msg.resolve ? (promise && promise.resolve) : (promise && promise.reject);\n")
	sb.WriteString("            if (call) {\n")
	sb.WriteString("                let call_args = Array.isArray(msg.arguments) ? msg.arguments : [];\n")
	sb.WriteString("                // Rejections carry an optional message string; surface it as an Error.\n")
	sb.WriteString("                if (!msg.resolve) {\n")
	sb.WriteString("                    call_args = [new Error(typeof call_args[0] === \"string\" ? call_args[0] : \"WebExtension native call failed\")];\n")
	sb.WriteString("                }\n")
	sb.WriteString("                call(...call_args);\n")
	sb.WriteString("            }\n")
	sb.WriteString("            if (promise) {\n")
	sb.WriteString("                delete this.promiseRegistry[msg.promise];\n")
	sb.WriteString("            }\n")
	sb.WriteString("        },\n")
	// Chrome callback-convention error contract: on a failed native call the
	// callback must still be invoked, with runtime.lastError set for its
	// duration and deleted after (undefined when idle, per the Chrome docs).
	// lastError is a getter (as in Chrome) so an unchecked error can be
	// logged after the callback returns. It is mirrored on both runtime
	// instances — Firefox defines browser.runtime.lastError too, and our
	// namespaces are separate objects. The promise convention doesn't use
	// lastError: the rejected promise is returned to the caller.
	sb.WriteString("        callbackWithError(callback, err) {\n")
	sb.WriteString("            const runtimes = [];\n")
	sb.WriteString("            if (window.chrome && window.chrome.runtime) runtimes.push(window.chrome.runtime);\n")
	sb.WriteString("            if (window.browser && window.browser.runtime && window.browser.runtime !== (window.chrome && window.chrome.runtime)) runtimes.push(window.browser.runtime);\n")
	sb.WriteString("            const lastError = { message: (err && err.message) ? err.message : String(err) };\n")
	sb.WriteString("            let checked = false;\n")
	sb.WriteString("            let usedV2LastError = false;\n")
	sb.WriteString("            for (const rt of runtimes) {\n")
	sb.WriteString("                const descriptor = Object.getOwnPropertyDescriptor(rt, \"lastError\");\n")
	sb.WriteString("                if (descriptor && !descriptor.configurable) {\n")
	sb.WriteString("                    if (window.__bridge_v2__) {\n")
	sb.WriteString("                        window.__bridge_v2__.lastError = lastError;\n")
	sb.WriteString("                        usedV2LastError = true;\n")
	sb.WriteString("                    }\n")
	sb.WriteString("                    checked = true;\n")
	sb.WriteString("                    continue;\n")
	sb.WriteString("                }\n")
	sb.WriteString("                Object.defineProperty(rt, \"lastError\", {\n")
	sb.WriteString("                    configurable: true,\n")
	sb.WriteString("                    get() { checked = true; return lastError; },\n")
	sb.WriteString("                });\n")
	sb.WriteString("            }\n")
	sb.WriteString("            try {\n")
	sb.WriteString("                callback();\n")
	sb.WriteString("            } finally {\n")
	sb.WriteString("                for (const rt of runtimes) {\n")
	sb.WriteString("                    const descriptor = Object.getOwnPropertyDescriptor(rt, \"lastError\");\n")
	sb.WriteString("                    if (!descriptor || descriptor.configurable) delete rt.lastError;\n")
	sb.WriteString("                }\n")
	sb.WriteString("                if (usedV2LastError) window.__bridge_v2__.lastError = null;\n")
	sb.WriteString("                if (!checked) {\n")
	sb.WriteString("                    console.error(\"Unchecked runtime.lastError: \" + lastError.message);\n")
	sb.WriteString("                }\n")
	sb.WriteString("            }\n")
	sb.WriteString("        },\n")
	sb.WriteString("        listenerRegistry: {},\n")
	sb.WriteString("        nextListenerId: 1,\n")
	sb.WriteString("        externs: {},\n")
	sb.WriteString("        addListener(className, eventName, listener, decoders) {\n")
	sb.WriteString("            const listenerId = this.nextListenerId++;\n")
	sb.WriteString("            this.listenerRegistry[listenerId] = { class: className, event: eventName, listener: listener, decoders: decoders || null };\n")
	sb.WriteString("            const payload = { class: className, event: eventName, command: \"add\", listener: listenerId, };\n")
	sb.WriteString("            window.webkit.messageHandlers[this.nativeEventHandler].postMessage(JSON.stringify(payload));\n")
	sb.WriteString("        },\n")
	sb.WriteString("        removeListener(className, eventName, listener) {\n")
	sb.WriteString("            const entry = Object.entries(this.listenerRegistry).find(([key, value]) => { return value.listener === listener && value.class == className && value.event == eventName; });\n")
	sb.WriteString("            if (entry) {\n")
	sb.WriteString("                const listenerId = entry[0];\n")
	sb.WriteString("                delete this.listenerRegistry[listenerId];\n")
	sb.WriteString("                const payload = { class: className, event: eventName, command: \"remove\", listener: +listenerId, };\n")
	sb.WriteString("                window.webkit.messageHandlers[this.nativeEventHandler].postMessage(JSON.stringify(payload));\n")
	sb.WriteString("            }\n")
	sb.WriteString("        },\n")
	sb.WriteString("        hasListener(className, eventName, listener) {\n")
	sb.WriteString("            const entry = Object.entries(this.listenerRegistry).find(([key, value]) => { return value.listener === listener && value.class == className && value.event == eventName; });\n")
	sb.WriteString("            return entry ? true : false;\n")
	sb.WriteString("        },\n")
	sb.WriteString("        handleNotify(...args) {\n")
	sb.WriteString("            const msg = this.parseJson(args[0]);\n")
	sb.WriteString("            if (!msg) {\n")
	sb.WriteString("                return;\n")
	sb.WriteString("            }\n")
	sb.WriteString("            const listener = this.listenerRegistry[msg.listener];\n")
	sb.WriteString("            const call = listener && listener.listener;\n")
	sb.WriteString("            if (call) {\n")
	sb.WriteString("                let call_args = Array.isArray(msg.arguments) ? msg.arguments : [];\n")
	sb.WriteString("                if (Array.isArray(call_args) && listener.decoders) {\n")
	sb.WriteString("                    call_args = call_args.map((v, i) => listener.decoders[i] ? listener.decoders[i](v) : v);\n")
	sb.WriteString("                }\n")
	sb.WriteString("                call(...call_args);\n")
	sb.WriteString("            }\n")
	sb.WriteString("        },\n")
	sb.WriteString("    };\n\n")
	sb.WriteString("    function createListener(className, eventName, decoders) {\n")
	sb.WriteString("        return {\n")
	sb.WriteString("            addListener(listener) { __bridge__.addListener(className, eventName, listener, decoders); },\n")
	sb.WriteString("            removeListener(listener) { __bridge__.removeListener(className, eventName, listener); },\n")
	sb.WriteString("            hasListener(listener) { return __bridge__.hasListener(className, eventName, listener); },\n")
	sb.WriteString("        };\n")
	sb.WriteString("    }\n\n")

	// ---- service factories ----
	for _, s := range idl.Services {
		fmt.Fprintf(&sb, "    function create%s(callConvention) {\n", s.Name)
		fmt.Fprintf(&sb, "        const instance = {};\n")

		if len(s.Methods) > 0 {
			sb.WriteString("\n")
			methodGroups := make(map[string][]*Rpc)
			methodOrder := make([]string, 0, len(s.Methods))
			for idx := range s.Methods {
				rpc := &s.Methods[idx]
				if _, ok := methodGroups[rpc.Name]; !ok {
					methodOrder = append(methodOrder, rpc.Name)
				}
				methodGroups[rpc.Name] = append(methodGroups[rpc.Name], rpc)
			}
			for _, methodName := range methodOrder {
				variants := methodGroups[methodName]
				fmt.Fprintf(&sb, "        instance.%s = (...args) => {\n", methodName)
				fmt.Fprintf(&sb, "            let callback = undefined;\n")
				fmt.Fprintf(&sb, "            let normalizedArgs = args.slice();\n")
				fmt.Fprintf(&sb, "            const matchesAnySignature = (candidateArgs) => ")
				for idx, variant := range variants {
					if idx > 0 {
						sb.WriteString(" || ")
					}
					signature := jsSignatureLiteral(jsSignatureForRpc(variant, s.Name, registry, messageSet))
					fmt.Fprintf(&sb, "__bridge__.matchSignature(candidateArgs, %s)", signature)
				}
				sb.WriteString(";\n")
				fmt.Fprintf(&sb, "            const lastArgument = normalizedArgs[normalizedArgs.length - 1];\n")
				fmt.Fprintf(&sb, "            if (typeof lastArgument === \"function\") {\n")
				fmt.Fprintf(&sb, "                callback = normalizedArgs.pop();\n")
				fmt.Fprintf(&sb, "            } else if ((lastArgument === null || lastArgument === undefined) && !matchesAnySignature(normalizedArgs)) {\n")
				fmt.Fprintf(&sb, "                const callbacklessArgs = normalizedArgs.slice(0, -1);\n")
				fmt.Fprintf(&sb, "                if (matchesAnySignature(callbacklessArgs)) {\n")
				fmt.Fprintf(&sb, "                    normalizedArgs = callbacklessArgs;\n")
				fmt.Fprintf(&sb, "                }\n")
				fmt.Fprintf(&sb, "            }\n")
				for _, variant := range variants {
					signature := jsSignatureLiteral(jsSignatureForRpc(variant, s.Name, registry, messageSet))
					fmt.Fprintf(&sb, "            {\n")
					fmt.Fprintf(&sb, "                if (__bridge__.matchSignature(normalizedArgs, %s)) {\n", signature)
					sb.WriteString("                    const marshalledArgs = [];\n")
					for idx, p := range variant.Params {
						if isStringStringMap(p.Type) {
							if p.Repeated {
								fmt.Fprintf(&sb, "                    marshalledArgs.push(Array.isArray(normalizedArgs[%d]) ? normalizedArgs[%d].map((item) => __bridge__.encodeStringMap(item)) : [__bridge__.encodeStringMap(normalizedArgs[%d])]);\n", idx, idx, idx)
							} else {
								fmt.Fprintf(&sb, "                    marshalledArgs.push(__bridge__.encodeStringMap(normalizedArgs[%d]));\n", idx)
							}
						} else if isAnyType(p.Type) {
							if p.Repeated {
								fmt.Fprintf(&sb, "                    marshalledArgs.push(Array.isArray(normalizedArgs[%d]) ? normalizedArgs[%d].map((item) => __bridge__.encodeAny(item)) : []);\n", idx, idx)
							} else {
								fmt.Fprintf(&sb, "                    marshalledArgs.push(__bridge__.encodeAny(normalizedArgs[%d]));\n", idx)
							}
						} else {
							fmt.Fprintf(&sb, "                    marshalledArgs.push(normalizedArgs[%d]);\n", idx)
						}
					}
					fmt.Fprintf(&sb, "                    let promise = __bridge__.postMessage(\"%s\", \"%s\", ...marshalledArgs);\n", s.Name, variant.BindingName)

					retType := strings.TrimSpace(variant.Return)
					isVoid := retType == "" || retType == "()"
					isRepeatedRet := false
					if strings.HasPrefix(retType, "repeatable ") {
						isRepeatedRet = true
						retType = strings.TrimSpace(strings.TrimPrefix(retType, "repeatable "))
					}
					if isStringAnyMap(retType) {
						if isRepeatedRet {
							sb.WriteString("                    promise = promise.then((value) => Array.isArray(value) ? value.map((item) => __bridge__.decodeAnyMap(item)) : value);\n")
						} else if !isVoid {
							sb.WriteString("                    promise = promise.then((value) => __bridge__.decodeAnyMap(value));\n")
						}
					} else if isAnyType(retType) {
						if isRepeatedRet {
							sb.WriteString("                    promise = promise.then((value) => Array.isArray(value) ? value.map((v) => __bridge__.decodeAny(v)) : value);\n")
						} else if !isVoid {
							sb.WriteString("                    promise = promise.then((value) => __bridge__.decodeAny(value));\n")
						}
					}
					sb.WriteString("                    if (callback) {\n")
					sb.WriteString("                        promise = promise.then(callback, (err) => __bridge__.callbackWithError(callback, err));\n")
					sb.WriteString("                        return;\n")
					sb.WriteString("                    }\n")
					sb.WriteString("                    return promise;\n")
					sb.WriteString("                }\n")
					sb.WriteString("            }\n")
				}
				fmt.Fprintf(&sb, "            throw new Error(`No overload of [${callConvention}]%s.%s matches provided arguments, args=${JSON.stringify(args)}`);\n", s.Name, methodName)
				sb.WriteString("        };\n")
			}
		}

		if len(s.Enums) > 0 {
			sb.WriteString("\n")
			for _, enumDef := range s.Enums {
				values, err := resolveEnumValuesForJS(enumDef, registry, nil)
				if err != nil {
					panic(err)
				}
				if len(values) == 0 {
					fmt.Fprintf(&sb, "        instance.%s = {};\n\n", enumDef.Name)
					continue
				}
				fmt.Fprintf(&sb, "        instance.%s = {\n", enumDef.Name)
				for i, value := range values {
					fmt.Fprintf(&sb, "            %s: %s", value.Name, strconv.Quote(value.Value))
					if i < len(values)-1 {
						sb.WriteString(",")
					}
					sb.WriteString("\n")
				}
				sb.WriteString("        };\n")
			}
		}

		if len(s.Listeners) > 0 {
			sb.WriteString("\n")
		}
		// Listeners
		for _, l := range s.Listeners {
			var decoderItems []string
			for _, p := range l.Params {
				if isAnyType(p.Type) {
					if p.Repeated {
						decoderItems = append(decoderItems, "(v) => Array.isArray(v) ? v.map(__bridge__.decodeAny) : v")
					} else {
						decoderItems = append(decoderItems, "__bridge__.decodeAny")
					}
				} else {
					decoderItems = append(decoderItems, "null")
				}
			}
			decoderLine := "null"
			if len(decoderItems) > 0 {
				decoderLine = "[" + strings.Join(decoderItems, ", ") + "]"
			}
			fmt.Fprintf(&sb, "        const %sDecoders = %s;\n", l.Name, decoderLine)
			fmt.Fprintf(&sb, "        instance.%s = createListener(\"%s\", \"%s\", %sDecoders);\n", l.Name, s.Name, l.Name, l.Name)
		}

		if len(s.Fields) > 0 {
			sb.WriteString("\n")
		}
		// Subservices
		for _, f := range s.Fields {
			fmt.Fprintf(&sb, "        instance.%s = create%s();\n", f.Name, f.Type)
		}

		if len(s.Externs) > 0 {
			sb.WriteString("\n")
			fmt.Fprintf(&sb, "        const externLink = (window.__extern__ && window.__extern__[\"%s\"]);\n", s.Name)
			sb.WriteString("        if(externLink) {\n")
			sb.WriteString("            externLink(instance, callConvention);\n")
			sb.WriteString("        }\n")
		}

		if len(s.Externs) > 0 {
			sb.WriteString("\n")
		}

		for _, e := range s.Externs {
			fmt.Fprintf(&sb, "        if (typeof instance.%s === \"undefined\") {\n", e)
			fmt.Fprintf(&sb, "            console.warn(\"Unresolved external symbol: %s.%s\");\n", s.Name, e)
			sb.WriteString("        }\n")
		}

		sb.WriteString("\n")
		sb.WriteString("        return instance;\n")
		sb.WriteString("    }\n\n")
	}

	sb.WriteString("    window.__bridge__ = __bridge__;\n\n")

	for _, s := range idl.Services {
		fmt.Fprintf(&sb, "    __bridge__.create%s = create%s;\n", s.Name, s.Name)
	}
	sb.WriteString("\n")

	// ---- globals ----
	browserCreatedObjects := make(map[string]bool)
	browserInitialized := false
	for _, s := range idl.Services {
		if s.Global == "" {
			continue
		}

		globalPath := s.Global
		if after, ok := strings.CutPrefix(globalPath, "browser."); ok {
			globalPath = after
		}

		if !browserInitialized {
			sb.WriteString("    window.browser = window.browser || {};\n")
			browserCreatedObjects["browser"] = true
			browserInitialized = true
		}

		parts := strings.Split(globalPath, ".")
		if len(parts) == 0 {
			continue
		}
		current := "browser"
		for i, rawPart := range parts {
			part := strings.TrimSpace(rawPart)
			if part == "" {
				continue
			}
			path := current + "." + part
			if i < len(parts)-1 {
				if !browserCreatedObjects[path] {
					fmt.Fprintf(&sb, "    %s = %s || {};\n", path, path)
					browserCreatedObjects[path] = true
				}
			} else {
				fmt.Fprintf(&sb, "    %s = create%s(`browser`);\n", path, s.Name)
			}
			current = path
		}
	}

	chromeCreatedObjects := make(map[string]bool)
	chromeInitialized := false
	for _, s := range idl.Services {
		if s.Global == "" {
			continue
		}

		globalPath := s.Global
		if after, ok := strings.CutPrefix(globalPath, "browser."); ok {
			globalPath = after
		}

		if !chromeInitialized {
			sb.WriteString("    window.chrome = window.chrome || {};\n")
			chromeCreatedObjects["chrome"] = true
			chromeInitialized = true
		}

		parts := strings.Split(globalPath, ".")
		if len(parts) == 0 {
			continue
		}
		current := "chrome"
		for i, rawPart := range parts {
			part := strings.TrimSpace(rawPart)
			if part == "" {
				continue
			}
			path := current + "." + part
			if i < len(parts)-1 {
				if !chromeCreatedObjects[path] {
					fmt.Fprintf(&sb, "    %s = %s || {};\n", path, path)
					chromeCreatedObjects[path] = true
				}
			} else {
				fmt.Fprintf(&sb, "    %s = create%s(`chrome`);\n", path, s.Name)
			}
			current = path
		}
	}

	sb.WriteString("})();\n")
	return sb.String()
}

func genCallCpp(idl *IDL) string {
	var sb strings.Builder
	registry := buildEnumRegistry(idl)
	sb.WriteString("namespace WebExtensions {\n")
	sb.WriteString("\n")
	sb.WriteString("bool native_call_bridge_received(int promise, const std::string& clazz, const std::string& function, const std::string& arguments_json, orion::bridge::WebViewBridge& webview, const std::string& extension_id, const std::string& script_world, const Implementations& impls, std::string& error) {\n")

	for _, s := range idl.Services {
		fmt.Fprintf(&sb, "    if(clazz == \"%s\") {\n", s.Name)
		fmt.Fprintf(&sb, "        //orion::log_debug(\"WebExtensions: Found class '%%s'\", clazz.c_str());\n")
		fmt.Fprintf(&sb, "        if(impls.%s == nullptr) {\n", strings.ToLower(s.Name))
		fmt.Fprintf(&sb, "            orion::log_debug(\"WebExtensions: No implementation for class '%%s'\", clazz.c_str());\n")
		fmt.Fprintf(&sb, "            return false;\n")
		fmt.Fprintf(&sb, "        }\n")
		fmt.Fprintf(&sb, "        [[maybe_unused]] %s& object = *impls.%s;\n", s.Name, strings.ToLower(s.Name))

		for _, rpc := range s.Methods {
			fmt.Fprintf(&sb, "        if(function == \"%s\") {\n", rpc.BindingName)
			fmt.Fprintf(&sb, "            //orion::log_debug(\"WebExtensions: Found function '%%s'\", \"%s\");\n", rpc.BindingName)

			// Determine return type
			ret := rpc.Return
			promiseType := "void"
			repeated := false
			if ret != "" && ret != "()" {
				if strings.HasPrefix(ret, "repeatable ") {
					repeated = true
					ret = strings.TrimPrefix(ret, "repeatable ")
				}
				promiseType = cppType(ret, repeated, s.Name, registry)
			}

			if len(rpc.Params) > 0 {
				argTypes := make([]string, 0, len(rpc.Params))
				for _, p := range rpc.Params {
					argTypes = append(argTypes, "std::optional<"+cppType(p.Type, p.Repeated, s.Name, registry)+">")
				}
				fmt.Fprintf(&sb, "            using ArgsTuple = std::tuple<%s>;\n", strings.Join(argTypes, ", "))
				sb.WriteString("            auto args_result = rfl::json::read<ArgsTuple>(arguments_json);\n")
				sb.WriteString("            if(!args_result) {\n")
				sb.WriteString("                error = \"Type Error: \" + args_result.error().what();\n")
				sb.WriteString("                return false;\n")
				sb.WriteString("            }\n")
				sb.WriteString("            auto args = std::move(*args_result);\n")
			}

			// Build call
			fmt.Fprintf(&sb, "            object.%s(extension_id, Promise<%s>{webview, promise, script_world}", rpc.Name, promiseType)
			for idx := range rpc.Params {
				fmt.Fprintf(&sb, ", std::get<%d>(args)", idx)
			}
			sb.WriteString(");\n")
			sb.WriteString("            return true;\n")
			sb.WriteString("        }\n")
		}

		sb.WriteString("        error = \"Function '\" + function + \"' not found\";\n")
		sb.WriteString("        return false;\n")
		sb.WriteString("    }\n")
	}

	sb.WriteString("    error = \"Class '\" + clazz + \"' not found\";\n")
	sb.WriteString("    return false;\n")
	sb.WriteString("}\n\n")
	sb.WriteString("}\n")
	return sb.String()
}

func genEventCpp(idl *IDL) string {
	var sb strings.Builder
	sb.WriteString("namespace WebExtensions {\n\n")
	sb.WriteString("bool native_event_bridge_received(int listener, const std::string& clazz, const std::string& event, const std::string& command, orion::bridge::WebViewBridge& webview, const std::string& extension_id, const std::string& script_world, const Implementations& impls, std::string& error) {\n")
	sb.WriteString("    (void)extension_id;\n")

	for _, s := range idl.Services {
		fmt.Fprintf(&sb, "    if(clazz == \"%s\") {\n", s.Name)
		fmt.Fprintf(&sb, "        //orion::log_debug(\"WebExtensions: Found class '%%s'\", clazz.c_str());\n")
		fmt.Fprintf(&sb, "        if(impls.%s == nullptr) {\n", strings.ToLower(s.Name))
		fmt.Fprintf(&sb, "            orion::log_debug(\"WebExtensions: No implementation for class '%%s'\", clazz.c_str());\n")
		fmt.Fprintf(&sb, "            return false;\n")
		fmt.Fprintf(&sb, "        }\n")
		fmt.Fprintf(&sb, "        [[maybe_unused]] %s& object = *impls.%s;\n", s.Name, strings.ToLower(s.Name))

		for _, l := range s.Listeners {
			fmt.Fprintf(&sb, "        if(event == \"%s\") {\n", l.Name)
			fmt.Fprintf(&sb, "            //orion::log_debug(\"WebExtensions: Found event '%%s'\", \"%s\");\n", l.Name)
			sb.WriteString("            if(command == \"add\") {\n")
			sb.WriteString("                object.")
			sb.WriteString(l.Name)
			sb.WriteString(".addListener(webview, listener, script_world);\n")
			sb.WriteString("                return true;\n")
			sb.WriteString("            }\n")
			sb.WriteString("            if(command == \"remove\") {\n")
			sb.WriteString("                object.")
			sb.WriteString(l.Name)
			sb.WriteString(".removeListener(webview, listener);\n")
			sb.WriteString("                return true;\n")
			sb.WriteString("            }\n")
			sb.WriteString("            return true;\n")
			sb.WriteString("        }\n")
		}

		sb.WriteString("        error = \"Event '\" + event + \"' not found\";\n")
		sb.WriteString("        return false;\n")
		sb.WriteString("    }\n")
	}

	sb.WriteString("    error = \"Class '\" + clazz + \"' not found\";\n")
	sb.WriteString("    return false;\n")
	sb.WriteString("}\n\n")
	sb.WriteString("}\n")
	return sb.String()
}

func genCommonCpp(idl *IDL) string {
	var sb strings.Builder
	sb.WriteString(
		`
#include "common/src/log.h"
#include "common/src/memory.h"

#include <sstream>
#include <iomanip>
#include <optional>
#include <memory>
#include <vector>
#include <tuple>
#include <utility>

#include "core/webkit_bridge/API/bridge.h"
#include "webkit_bridge_api.h"
`)
	return sb.String()
}

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

func genBridgeJsVerbatim(idl *IDL) string {
	var sb strings.Builder
	const chunkSize = 16 * 1024
	bridgeJs := "\n" + genBridgeJs(idl) + "\n"

	sb.WriteString("static const char* bridge_js =\n")
	chunks := splitStringChunks(bridgeJs, chunkSize)
	for i, chunk := range chunks {
		sb.WriteString("R\"{verbatim}(")
		sb.WriteString(chunk)
		sb.WriteString("){verbatim}\"")
		if i < len(chunks)-1 {
			sb.WriteString("\n")
		} else {
			sb.WriteString(";\n")
		}
	}
	sb.WriteString(
		`
namespace WebExtensions {

const char* native_bridge_script() {
    return bridge_js;
}

void native_call_not_implemented(const char* call) {
    orion::log_warning("WebExtensions> %s> NOT IMPLEMENTED", call);
}

}
`)
	return sb.String()
}

func main() {
	// --- parse flags ---
	prefix := flag.String("prefix", "go", "output filename prefix")
	flag.Parse()

	// remaining args are IDL files
	files := flag.Args()
	if len(files) == 0 {
		fmt.Fprintln(os.Stderr, "Usage: go run generate.go --prefix=<file_prefix> file1.idl [file2.idl ...]")
		os.Exit(1)
	}

	idl, err := parseIDLFiles(files)
	if err != nil {
		panic(err)
	}
	assignRpcBindingNames(idl)
	//printIDL(idl)

	// --- single-file generators ---
	singleGenerators := []struct {
		name string
		gen  func(*IDL) string
	}{
		{"api.h", genApiHeader},
	}

	for _, g := range singleGenerators {
		filename := *prefix + "_" + g.name
		content := g.gen(idl)
		fmt.Println("Generating", filename, "...")
		if err := os.WriteFile(filename, []byte(content), 0644); err != nil {
			panic(err)
		}
	}

	// --- combined C++ impl file ---
	implGenerators := []struct {
		name string
		gen  func(*IDL) string
	}{
		{"common", genCommonCpp},
		{"call", genCallCpp},
		{"event", genEventCpp},
		{"js", genBridgeJsVerbatim},
	}

	implFile := *prefix + "_impl.cpp"
	var impl strings.Builder
	for _, g := range implGenerators {
		fmt.Println("Adding", g.name, "to", implFile, "...")
		impl.WriteString("// ===== " + g.name + " =====\n\n")
		impl.WriteString(g.gen(idl))
		impl.WriteString("\n\n")
	}

	fmt.Println("Generating", implFile, "...")
	if err := os.WriteFile(implFile, []byte(impl.String()), 0644); err != nil {
		panic(err)
	}
}
