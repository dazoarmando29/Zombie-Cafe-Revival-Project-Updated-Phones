package registry

import (
	"encoding/json"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func writeData(t *testing.T, dir, name, content string) {
	t.Helper()
	if err := os.WriteFile(filepath.Join(dir, name), []byte(content), 0644); err != nil {
		t.Fatal(err)
	}
}

const sampleChars = `[
    {
        "CafeLevelRequired": 0,
        "Name": "Man",
        "CharacterArtStringHead": "generic"
    },
    {
        "CafeLevelRequired": 0,
        "Name": "Senior!",
        "CharacterArtStringHead": "oldman"
    }
]`

func TestValidID(t *testing.T) {
	good := []string{"char.man", "recipe.brain_burger", "furniture.haunted_table_2", "a.b.c"}
	bad := []string{"", "Char.man", "man", "1char.x", "char.", "char..x", "char.Man", "char man"}
	for _, s := range good {
		if !ValidID(s) {
			t.Errorf("ValidID(%q) = false, want true", s)
		}
	}
	for _, s := range bad {
		if ValidID(s) {
			t.Errorf("ValidID(%q) = true, want false", s)
		}
	}
}

func TestAssignMissingIDs(t *testing.T) {
	dir := t.TempDir()
	writeData(t, dir, "characterData.bin.mid.json", sampleChars)
	writeData(t, dir, "foodData.bin.mid.json", `[]`)
	writeData(t, dir, "furnitureData.bin.mid.json", `[]`)

	n, res := AssignMissingIDs(dir)
	if res.Errors() != 0 {
		t.Fatalf("assign issues: %v", res.Issues)
	}
	if n != 2 {
		t.Fatalf("assigned %d ids, want 2", n)
	}

	rows, res := LoadRows(filepath.Join(dir, "characterData.bin.mid.json"))
	if res.Errors() != 0 {
		t.Fatalf("load issues: %v", res.Issues)
	}
	if rows[0].ID != "char.man" || rows[1].ID != "char.senior" {
		t.Fatalf("ids = %q, %q", rows[0].ID, rows[1].ID)
	}

	// formatting preserved: original fields still byte-identical
	data, _ := os.ReadFile(filepath.Join(dir, "characterData.bin.mid.json"))
	s := string(data)
	if !strings.Contains(s, `"CharacterArtStringHead": "generic"`) ||
		!strings.Contains(s, `"Name": "Senior!"`) {
		t.Fatalf("row content lost:\n%s", s)
	}

	// idempotent: running again assigns nothing
	n2, _ := AssignMissingIDs(dir)
	if n2 != 0 {
		t.Fatalf("second run assigned %d ids, want 0", n2)
	}
}

func TestValidateDuplicateAndLock(t *testing.T) {
	dir := t.TempDir()
	writeData(t, dir, "characterData.bin.mid.json", `[
        {"Name": "A", "ZcrId": "char.a"},
        {"Name": "B", "ZcrId": "char.a"}
    ]`)
	writeData(t, dir, "foodData.bin.mid.json", `[]`)
	writeData(t, dir, "furnitureData.bin.mid.json", `[]`)

	res := Validate(dir, filepath.Join(dir, "lock.json"))
	if res.Errors() == 0 {
		t.Fatal("duplicate id not detected")
	}

	writeData(t, dir, "characterData.bin.mid.json", `[
        {"Name": "A", "ZcrId": "char.a"},
        {"Name": "B", "ZcrId": "char.b"}
    ]`)
	lf, res := BuildLock(dir)
	if res.Errors() != 0 {
		t.Fatal(res.Issues)
	}
	lockPath := filepath.Join(dir, "lock.json")
	if err := SaveLock(lockPath, lf); err != nil {
		t.Fatal(err)
	}

	// clean state validates
	res = Validate(dir, lockPath)
	if res.Errors() != 0 {
		t.Fatalf("clean validate: %v", res.Issues)
	}

	// reordering rows breaks the lock
	writeData(t, dir, "characterData.bin.mid.json", `[
        {"Name": "B", "ZcrId": "char.b"},
        {"Name": "A", "ZcrId": "char.a"}
    ]`)
	res = Validate(dir, lockPath)
	if res.Errors() == 0 {
		t.Fatal("row reorder not detected")
	}

	// removing a locked row breaks the lock
	writeData(t, dir, "characterData.bin.mid.json", `[
        {"Name": "A", "ZcrId": "char.a"}
    ]`)
	res = Validate(dir, lockPath)
	if res.Errors() == 0 {
		t.Fatal("row removal not detected")
	}
}

func TestDependencies(t *testing.T) {
	dir := t.TempDir()
	writeData(t, dir, "characterData.bin.mid.json", `[
        {"Name": "A", "ZcrId": "char.a", "ZcrDependsOn": ["recipe.soup"]},
        {"Name": "B", "ZcrId": "char.b", "ZcrDependsOn": ["recipe.missing"]}
    ]`)
	writeData(t, dir, "foodData.bin.mid.json", `[
        {"Name": "Soup", "ZcrId": "recipe.soup"}
    ]`)
	writeData(t, dir, "furnitureData.bin.mid.json", `[]`)

	res := Validate(dir, filepath.Join(dir, "lock.json"))
	found := false
	for _, is := range res.Issues {
		if strings.Contains(is.Message, "recipe.missing") {
			found = true
		}
	}
	if !found {
		t.Fatalf("missing dependency not reported: %v", res.Issues)
	}

	// cycle detection
	writeData(t, dir, "characterData.bin.mid.json", `[
        {"Name": "A", "ZcrId": "char.a", "ZcrDependsOn": ["char.b"]},
        {"Name": "B", "ZcrId": "char.b", "ZcrDependsOn": ["char.a"]}
    ]`)
	res = Validate(dir, filepath.Join(dir, "lock.json"))
	found = false
	for _, is := range res.Issues {
		if strings.Contains(is.Message, "cycle") {
			found = true
		}
	}
	if !found {
		t.Fatalf("cycle not reported: %v", res.Issues)
	}
}

func TestLockRoundTrip(t *testing.T) {
	dir := t.TempDir()
	writeData(t, dir, "characterData.bin.mid.json", `[
        {"Name": "A", "ZcrId": "char.a"}
    ]`)
	writeData(t, dir, "foodData.bin.mid.json", `[]`)
	writeData(t, dir, "furnitureData.bin.mid.json", `[]`)

	lf, _ := BuildLock(dir)
	p := filepath.Join(dir, "lock.json")
	if err := SaveLock(p, lf); err != nil {
		t.Fatal(err)
	}
	lf2, res := LoadLock(p)
	if res.Errors() != 0 {
		t.Fatal(res.Issues)
	}
	if len(lf2.Entries) != 1 || lf2.Entries[0].ID != "char.a" {
		t.Fatalf("round trip: %+v", lf2.Entries)
	}
	// hash must be stable across marshalings
	var b1, b2 map[string]interface{}
	json.Unmarshal([]byte(`{"Name":"A","ZcrId":"char.a"}`), &b1)
	json.Unmarshal([]byte(`{"ZcrId":"char.a","Name":"A"}`), &b2)
	if hashRow(b1) != hashRow(b2) {
		t.Fatal("hash depends on key order")
	}
}
