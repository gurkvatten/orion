package main

import (
	"archive/zip"
	"crypto"
	"crypto/rand"
	"crypto/rsa"
	"crypto/sha1"
	"crypto/x509"
	"encoding/binary"
	"encoding/pem"
	"errors"
	"flag"
	"fmt"
	"io"
	"io/fs"
	"os"
	"path/filepath"
	"runtime"
	"sort"
	"strings"
)

// overlayMarker is the per-flavor file that names a shared source directory
// (read by PackageWebExtensions.cmake, which passes both as --source). It is
// never written into the archive.
const overlayMarker = ".orion-overlay"

// sourceList collects repeated --source flags. Sources are layered in order:
// a file in a later directory replaces the same relative path from an
// earlier one, so a flavor directory can override a shared body's files.
type sourceList []string

func (s *sourceList) String() string     { return strings.Join(*s, string(os.PathListSeparator)) }
func (s *sourceList) Set(value string) error { *s = append(*s, value); return nil }

func main() {
	archiveFormat := flag.String("format", "", "output format: crx2 or zip")
	var sources sourceList
	flag.Var(&sources, "source", "path to an unpacked extension directory; repeatable, later directories override earlier ones")
	outputPath := flag.String("output", "", "path to the output archive")
	keyPath := flag.String("key", "", "path to the persistent RSA private key")
	flag.Parse()

	if *archiveFormat == "" || len(sources) == 0 || *outputPath == "" {
		flag.Usage()
		os.Exit(2)
	}

	var err error
	switch *archiveFormat {
	case "crx2":
		if *keyPath == "" {
			fmt.Fprintln(os.Stderr, "--key is required for CRX2 output")
			os.Exit(2)
		}
		err = packageCRX2(sources, *outputPath, *keyPath)
	case "zip":
		err = packageZIP(sources, *outputPath)
	default:
		fmt.Fprintf(os.Stderr, "unsupported output format %q\n", *archiveFormat)
		os.Exit(2)
	}
	if err != nil {
		fmt.Fprintf(os.Stderr, "package WebExtension: %v\n", err)
		os.Exit(1)
	}
}

func packageZIP(sources []string, outputPath string) error {
	if err := os.MkdirAll(filepath.Dir(outputPath), 0o755); err != nil {
		return fmt.Errorf("create ZIP output directory: %w", err)
	}
	temporary, err := os.CreateTemp(filepath.Dir(outputPath), ".package-webextension-*.zip")
	if err != nil {
		return fmt.Errorf("create temporary ZIP archive: %w", err)
	}
	temporaryPath := temporary.Name()
	defer os.Remove(temporaryPath)
	if err := temporary.Close(); err != nil {
		return fmt.Errorf("close temporary ZIP archive: %w", err)
	}

	if err := createZIPArchive(sources, temporaryPath); err != nil {
		return err
	}
	if err := replaceFile(temporaryPath, outputPath); err != nil {
		return fmt.Errorf("install ZIP archive: %w", err)
	}
	return nil
}

func packageCRX2(sources []string, crxPath, keyPath string) error {
	privateKey, err := loadOrCreatePrivateKey(keyPath)
	if err != nil {
		return err
	}

	if err := os.MkdirAll(filepath.Dir(crxPath), 0o755); err != nil {
		return fmt.Errorf("create CRX output directory: %w", err)
	}
	zipFile, err := os.CreateTemp(filepath.Dir(crxPath), ".package-crx2-*.zip")
	if err != nil {
		return fmt.Errorf("create temporary ZIP archive: %w", err)
	}
	zipPath := zipFile.Name()
	defer os.Remove(zipPath)
	if err := zipFile.Close(); err != nil {
		return fmt.Errorf("close temporary ZIP archive: %w", err)
	}
	if err := createZIPArchive(sources, zipPath); err != nil {
		return err
	}

	zipFile, err = os.Open(zipPath)
	if err != nil {
		return fmt.Errorf("open temporary ZIP archive: %w", err)
	}
	defer zipFile.Close()

	digest := sha1.New()
	if _, err := io.Copy(digest, zipFile); err != nil {
		return fmt.Errorf("hash ZIP archive: %w", err)
	}
	if _, err := zipFile.Seek(0, io.SeekStart); err != nil {
		return fmt.Errorf("rewind ZIP archive: %w", err)
	}

	signature, err := rsa.SignPKCS1v15(rand.Reader, privateKey, crypto.SHA1, digest.Sum(nil))
	if err != nil {
		return fmt.Errorf("sign ZIP archive: %w", err)
	}
	publicKey, err := x509.MarshalPKIXPublicKey(&privateKey.PublicKey)
	if err != nil {
		return fmt.Errorf("marshal public key: %w", err)
	}

	output, err := os.CreateTemp(filepath.Dir(crxPath), ".package-crx2-*")
	if err != nil {
		return fmt.Errorf("create temporary CRX archive: %w", err)
	}
	temporaryPath := output.Name()
	defer os.Remove(temporaryPath)

	writeError := writeCRX2(output, publicKey, signature, zipFile)
	closeError := output.Close()
	zipCloseError := zipFile.Close()
	if writeError != nil {
		return writeError
	}
	if closeError != nil {
		return fmt.Errorf("close temporary CRX archive: %w", closeError)
	}
	if zipCloseError != nil {
		return fmt.Errorf("close ZIP archive: %w", zipCloseError)
	}

	if err := replaceFile(temporaryPath, crxPath); err != nil {
		return fmt.Errorf("install CRX archive: %w", err)
	}
	return nil
}

// collectEntries maps archive paths to files on disk across all sources; a
// later source overrides an earlier one for the same relative path.
func collectEntries(sources []string) (map[string]string, error) {
	entries := map[string]string{}
	for _, sourcePath := range sources {
		walkError := filepath.WalkDir(sourcePath, func(path string, entry fs.DirEntry, walkError error) error {
			if walkError != nil {
				return walkError
			}
			if entry.IsDir() {
				return nil
			}
			if !entry.Type().IsRegular() {
				return fmt.Errorf("unsupported extension entry %q", path)
			}
			if entry.Name() == overlayMarker {
				return nil
			}
			relativePath, err := filepath.Rel(sourcePath, path)
			if err != nil {
				return fmt.Errorf("make ZIP path relative: %w", err)
			}
			entries[filepath.ToSlash(relativePath)] = path
			return nil
		})
		if walkError != nil {
			return nil, fmt.Errorf("walk extension source %q: %w", sourcePath, walkError)
		}
	}
	return entries, nil
}

func createZIPArchive(sources []string, zipPath string) error {
	entries, err := collectEntries(sources)
	if err != nil {
		return err
	}
	names := make([]string, 0, len(entries))
	for name := range entries {
		names = append(names, name)
	}
	sort.Strings(names) // deterministic archive order regardless of walk order

	output, err := os.Create(zipPath)
	if err != nil {
		return fmt.Errorf("create ZIP archive: %w", err)
	}
	archive := zip.NewWriter(output)

	var writeError error
	for _, name := range names {
		if writeError = addZIPEntry(archive, name, entries[name]); writeError != nil {
			break
		}
	}
	archiveCloseError := archive.Close()
	outputCloseError := output.Close()
	if writeError != nil {
		return writeError
	}
	if archiveCloseError != nil {
		return fmt.Errorf("close ZIP archive: %w", archiveCloseError)
	}
	if outputCloseError != nil {
		return fmt.Errorf("close ZIP output: %w", outputCloseError)
	}
	return nil
}

func addZIPEntry(archive *zip.Writer, name, path string) error {
	info, err := os.Stat(path)
	if err != nil {
		return fmt.Errorf("read extension entry info: %w", err)
	}
	header, err := zip.FileInfoHeader(info)
	if err != nil {
		return fmt.Errorf("create ZIP entry header: %w", err)
	}
	header.Name = name
	header.Method = zip.Deflate

	entryWriter, err := archive.CreateHeader(header)
	if err != nil {
		return fmt.Errorf("create ZIP entry %q: %w", name, err)
	}
	input, err := os.Open(path)
	if err != nil {
		return fmt.Errorf("open extension entry %q: %w", name, err)
	}
	_, copyError := io.Copy(entryWriter, input)
	closeError := input.Close()
	if copyError != nil {
		return fmt.Errorf("write ZIP entry %q: %w", name, copyError)
	}
	if closeError != nil {
		return fmt.Errorf("close extension entry %q: %w", name, closeError)
	}
	return nil
}

func writeCRX2(output io.Writer, publicKey, signature []byte, zipFile io.Reader) error {
	if _, err := io.WriteString(output, "Cr24"); err != nil {
		return fmt.Errorf("write CRX magic: %w", err)
	}
	for _, value := range []uint32{2, uint32(len(publicKey)), uint32(len(signature))} {
		if err := binary.Write(output, binary.LittleEndian, value); err != nil {
			return fmt.Errorf("write CRX header: %w", err)
		}
	}
	if _, err := output.Write(publicKey); err != nil {
		return fmt.Errorf("write CRX public key: %w", err)
	}
	if _, err := output.Write(signature); err != nil {
		return fmt.Errorf("write CRX signature: %w", err)
	}
	if _, err := io.Copy(output, zipFile); err != nil {
		return fmt.Errorf("write CRX ZIP payload: %w", err)
	}
	return nil
}

func loadOrCreatePrivateKey(path string) (*rsa.PrivateKey, error) {
	data, err := os.ReadFile(path)
	if err == nil && len(data) > 0 {
		key, err := parsePrivateKey(data)
		if err != nil {
			return nil, fmt.Errorf("parse private key %q: %w", path, err)
		}
		return key, nil
	}
	if err != nil && !errors.Is(err, fs.ErrNotExist) {
		return nil, fmt.Errorf("read private key %q: %w", path, err)
	}

	key, err := rsa.GenerateKey(rand.Reader, 2048)
	if err != nil {
		return nil, fmt.Errorf("generate private key: %w", err)
	}
	encoded, err := x509.MarshalPKCS8PrivateKey(key)
	if err != nil {
		return nil, fmt.Errorf("marshal private key: %w", err)
	}
	if err := writePrivateKey(path, pem.EncodeToMemory(&pem.Block{
		Type:  "PRIVATE KEY",
		Bytes: encoded,
	})); err != nil {
		return nil, err
	}
	return key, nil
}

func parsePrivateKey(data []byte) (*rsa.PrivateKey, error) {
	block, _ := pem.Decode(data)
	if block == nil {
		return nil, errors.New("missing PEM block")
	}
	if key, err := x509.ParsePKCS1PrivateKey(block.Bytes); err == nil {
		return key, key.Validate()
	}
	parsed, err := x509.ParsePKCS8PrivateKey(block.Bytes)
	if err != nil {
		return nil, err
	}
	key, ok := parsed.(*rsa.PrivateKey)
	if !ok {
		return nil, fmt.Errorf("expected RSA key, got %T", parsed)
	}
	return key, key.Validate()
}

func writePrivateKey(path string, data []byte) error {
	directory := filepath.Dir(path)
	if err := os.MkdirAll(directory, 0o700); err != nil {
		return fmt.Errorf("create private key directory: %w", err)
	}
	temporary, err := os.CreateTemp(directory, ".package-crx2-key-*")
	if err != nil {
		return fmt.Errorf("create temporary private key: %w", err)
	}
	temporaryPath := temporary.Name()
	defer os.Remove(temporaryPath)

	if err := temporary.Chmod(0o600); err != nil {
		temporary.Close()
		return fmt.Errorf("set private key permissions: %w", err)
	}
	if _, err := temporary.Write(data); err != nil {
		temporary.Close()
		return fmt.Errorf("write private key: %w", err)
	}
	if err := temporary.Close(); err != nil {
		return fmt.Errorf("close private key: %w", err)
	}
	if err := replaceFile(temporaryPath, path); err != nil {
		return fmt.Errorf("install private key: %w", err)
	}
	return nil
}

func replaceFile(source, destination string) error {
	if runtime.GOOS == "windows" {
		if err := os.Remove(destination); err != nil && !errors.Is(err, fs.ErrNotExist) {
			return err
		}
	}
	return os.Rename(source, destination)
}
