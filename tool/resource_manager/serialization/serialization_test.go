package serialization

import "testing"

func TestCharacterSheetOutputs(t *testing.T) {
	cases := []struct {
		folder  string
		variant string
		offs    string
		art     string
		scale   float32
		ok      bool
	}{
		{"characterParts", "en", "characterOffsets.bin.mid", "characterArt.bin.mid", 0.75, true},
		{"characterParts2", "en", "characterOffsets2.bin.mid", "characterArt2.bin.mid", 0.75, true},
		{"characterParts3", "en", "characterOffsets3.bin.mid", "characterArt3.bin.mid", 0.75, true},
		{"characterParts5", "jp", "characterOffsets5.bin.mid", "characterArt5.bin.mid", 1, true},
		{"characterParts12", "en", "characterOffsets12.bin.mid", "characterArt12.bin.mid", 0.75, true},
		{"furniture", "en", "", "", 0, false},
		{"characterPartsX", "en", "", "", 0, false},
		{"characterParts-3", "en", "", "", 0, false},
	}
	for _, c := range cases {
		out, ok := characterSheetOutputs(c.folder, c.variant)
		if ok != c.ok {
			t.Errorf("%s/%s: ok=%v want %v", c.folder, c.variant, ok, c.ok)
			continue
		}
		if !ok {
			continue
		}
		if out.textureData.offsetsName != c.offs {
			t.Errorf("%s: offsets %q want %q", c.folder, out.textureData.offsetsName, c.offs)
		}
		if out.characterArtFile != c.art {
			t.Errorf("%s: art %q want %q", c.folder, out.characterArtFile, c.art)
		}
		if out.textureData.scale != c.scale {
			t.Errorf("%s: scale %v want %v", c.folder, out.textureData.scale, c.scale)
		}
	}
}
