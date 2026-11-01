package validator

import (
	"encoding/binary"
	"encoding/json"
	"image"
	"image/color"
	"image/png"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"file_types"
)

func writePNG(t *testing.T, path string, w, h int) {
	t.Helper()
	img := image.NewNRGBA(image.Rect(0, 0, w, h))
	for y := 0; y < h; y++ {
		for x := 0; x < w; x++ {
			img.Set(x, y, color.NRGBA{uint8(x), uint8(y), 0, 255})
		}
	}
	os.MkdirAll(filepath.Dir(path), 0o755)
	f, err := os.Create(path)
	if err != nil {
		t.Fatal(err)
	}
	defer f.Close()
	if err := png.Encode(f, img); err != nil {
		t.Fatal(err)
	}
}

func writeJSONFile(t *testing.T, path string, v interface{}) {
	t.Helper()
	b, err := json.Marshal(v)
	if err != nil {
		t.Fatal(err)
	}
	os.MkdirAll(filepath.Dir(path), 0o755)
	if err := os.WriteFile(path, b, 0o644); err != nil {
		t.Fatal(err)
	}
}

func writeCCT(t *testing.T, path string, w, h int32, magic string) {
	t.Helper()
	os.MkdirAll(filepath.Dir(path), 0o755)
	f, err := os.Create(path)
	if err != nil {
		t.Fatal(err)
	}
	defer f.Close()
	f.WriteString(magic)
	binary.Write(f, binary.LittleEndian, int32(2))
	binary.Write(f, binary.LittleEndian, w)
	binary.Write(f, binary.LittleEndian, h)
	binary.Write(f, binary.LittleEndian, int32(0))
	binary.Write(f, binary.LittleEndian, int32(0))
	binary.Write(f, binary.LittleEndian, int32(0))
}

// build a minimal valid asset tree
func buildValidTree(t *testing.T) string {
	t.Helper()
	root := t.TempDir()
	chars := []file_types.Character{
		{Name: "Test A", CharacterArtStringHead: "alpha", CharacterArtString: "beta"},
	}
	writeJSONFile(t, filepath.Join(root, "data", "characterData.bin.mid.json"), chars)
	writeJSONFile(t, filepath.Join(root, "data", "foodData.bin.mid.json"),
		[]file_types.Food{{Name: "Soup", CookTimeMinutes: 5}})
	writeJSONFile(t, filepath.Join(root, "data", "furnitureData.bin.mid.json"),
		[]file_types.Furniture{{Name: "Chair", SizeX: 1, SizeY: 1, ImagePackIndex: 0}})
	writeJSONFile(t, filepath.Join(root, "images", "furnitureOffsets.bin.mid.json"),
		file_types.ImageOffsets{Type: 2, Offsets: []file_types.Offset{{Name: "chair", X: 0, Y: 0, W: 8, H: 8}}})
	writeCCT(t, filepath.Join(root, "images", "furniture.cct.mid"), 64, 64, "CCTX")
	// two character folders, 3 pieces each
	for _, folder := range []string{"alpha-human", "beta-human"} {
		for i := 0; i < 3; i++ {
			writePNG(t, filepath.Join(root, "images", "characterParts", folder,
				string(rune('a'+i))+".png"), 4, 4)
		}
	}
	return root
}

func hasError(res *Result, substr string) bool {
	for _, is := range res.Issues {
		if is.Severity == SevError && strings.Contains(is.Message, substr) {
			return true
		}
	}
	return false
}

func TestValidTreePasses(t *testing.T) {
	root := buildValidTree(t)
	res := ValidateAssets(root)
	for _, is := range res.Issues {
		t.Log(is)
	}
	if res.Errors() != 0 {
		t.Fatalf("expected 0 errors, got %d", res.Errors())
	}
}

func TestCharacterCountLimit(t *testing.T) {
	root := buildValidTree(t)
	chars := make([]file_types.Character, 256)
	for i := range chars {
		chars[i] = file_types.Character{Name: "C", CharacterArtStringHead: "alpha", CharacterArtString: "beta"}
	}
	writeJSONFile(t, filepath.Join(root, "data", "characterData.bin.mid.json"), chars)
	res := ValidateAssets(root)
	if !hasError(res, "exceeds the 255 limit") {
		t.Fatalf("expected character-count error, issues: %v", res.Issues)
	}
}

func TestUnresolvedArtString(t *testing.T) {
	root := buildValidTree(t)
	chars := []file_types.Character{
		{Name: "Ghost", CharacterArtStringHead: "nosuchfolder", CharacterArtString: "beta"},
	}
	writeJSONFile(t, filepath.Join(root, "data", "characterData.bin.mid.json"), chars)
	res := ValidateAssets(root)
	if !hasError(res, "does not match any characterParts") {
		t.Fatalf("expected unresolved-art error, issues: %v", res.Issues)
	}
}

func TestInconsistentPieceCounts(t *testing.T) {
	root := buildValidTree(t)
	writePNG(t, filepath.Join(root, "images", "characterParts", "alpha-human", "extra.png"), 4, 4)
	res := ValidateAssets(root)
	if !hasError(res, "inconsistent piece count") {
		t.Fatalf("expected piece-count error, issues: %v", res.Issues)
	}
}

func TestBadOffsets(t *testing.T) {
	root := buildValidTree(t)
	writeJSONFile(t, filepath.Join(root, "images", "furnitureOffsets.bin.mid.json"),
		file_types.ImageOffsets{Type: 2, Offsets: []file_types.Offset{{Name: "bad", X: 0, Y: 0, W: 0, H: 5}}})
	res := ValidateAssets(root)
	if !hasError(res, "non-positive size") {
		t.Fatalf("expected rect-size error, issues: %v", res.Issues)
	}
}

func TestBadCCTMagic(t *testing.T) {
	root := buildValidTree(t)
	writeCCT(t, filepath.Join(root, "images", "furniture.cct.mid"), 64, 64, "BAD!")
	res := ValidateAssets(root)
	if !hasError(res, "bad magic") {
		t.Fatalf("expected magic error, issues: %v", res.Issues)
	}
}

func TestFoodCountLimit(t *testing.T) {
	root := buildValidTree(t)
	foods := make([]file_types.Food, 256)
	for i := range foods {
		foods[i] = file_types.Food{Name: "F"}
	}
	writeJSONFile(t, filepath.Join(root, "data", "foodData.bin.mid.json"), foods)
	res := ValidateAssets(root)
	if !hasError(res, "food count 256 exceeds") {
		t.Fatalf("expected food-count error, issues: %v", res.Issues)
	}
}

func TestEmptyCharacterName(t *testing.T) {
	root := buildValidTree(t)
	chars := []file_types.Character{
		{Name: "  ", CharacterArtStringHead: "alpha", CharacterArtString: "beta"},
	}
	writeJSONFile(t, filepath.Join(root, "data", "characterData.bin.mid.json"), chars)
	res := ValidateAssets(root)
	if !hasError(res, "empty Name") {
		t.Fatalf("expected empty-name error, issues: %v", res.Issues)
	}
}
