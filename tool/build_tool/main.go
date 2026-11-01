package main

import (
	"flag"
	"fmt"
	"io/ioutil"
	"log"
	"os"
	"path/filepath"
	"strings"
	"asset_validator/validator"
	"content_registry/registry"
	"tool/build_tool/copylist"
	"tool/resource_manager/serialization"
)

func main() {
	var in_directory string
	flag.StringVar(&in_directory, "i", "", "Path of source")

	var out_directory string
	flag.StringVar(&out_directory, "o", "", "Path of output directory")

	flag.Parse()

	if in_directory == "" {
		log.Fatalln("In directory not specified")
	}

	if out_directory == "" {
		log.Fatalln("Out directory not specified")
	}

	os.RemoveAll(out_directory)

	// validate the asset sources first: malformed content must fail the
	// build with a clear message instead of shipping a corrupt APK
	{
		res := validator.ValidateAssets(filepath.Join(in_directory, "assets"))
		for _, is := range res.Issues {
			log.Println(is)
		}
		if res.Errors() > 0 {
			log.Fatalf("asset validation failed with %d error(s)", res.Errors())
		}
	}

	// content registry gate: stable ids must exist, be unique and keep their
	// pinned rows (append-only) - see docs/CONTENT_REGISTRY.md
	{
		dataDir := filepath.Join(in_directory, "assets", "data")
		lockPath := filepath.Join(dataDir, "zcr_registry.lock.json")
		res := registry.Validate(dataDir, lockPath)
		for _, is := range res.Issues {
			log.Println(is)
		}
		if res.Errors() > 0 {
			log.Fatalf("content registry validation failed with %d error(s)", res.Errors())
		}
	}

	copyFiles(in_directory, out_directory)

	serialization.SerializeFiles(filepath.Join(in_directory, "assets", "data"), filepath.Join(out_directory, "assets", "data"))
	serialization.SerializeFiles(filepath.Join(in_directory, "assets", "images"), filepath.Join(out_directory, "assets", "images"))
	// pack every character sheet folder found (characterParts, characterParts2,
	// characterParts3, ...) - the pipeline is data driven, so a new sheet is
	// added by dropping in a new folder (engine-side loading is a separate
	// concern, see docs/ASSET_EXPANSION_GUIDE.md)
	if sheetDirs, _ := filepath.Glob(filepath.Join(in_directory, "assets", "images", "characterParts*")); len(sheetDirs) > 0 {
		for _, dir := range sheetDirs {
			if fi, err := os.Stat(dir); err == nil && fi.IsDir() {
				serialization.PackCharacters(dir, filepath.Join(out_directory, "assets", "images"), filepath.Join(out_directory, "assets", "data"), "en")
			}
		}
	}
	serialization.PackTextures(filepath.Join(in_directory, "assets", "images", "recipeImages"), filepath.Join(out_directory, "assets", "images"))
	serialization.PackTextures(filepath.Join(in_directory, "assets", "images", "recipeImages2"), filepath.Join(out_directory, "assets", "images"))
	serialization.PackTextures(filepath.Join(in_directory, "assets", "images", "furniture"), filepath.Join(out_directory, "assets", "images"))
	serialization.PackTextures(filepath.Join(in_directory, "assets", "images", "furniture2"), filepath.Join(out_directory, "assets", "images"))
	serialization.PackTextures(filepath.Join(in_directory, "assets", "images", "furniture3"), filepath.Join(out_directory, "assets", "images"))
	//serialization.PackTextures(filepath.Join(in_directory, "assets", "images", "mapTiles"), filepath.Join(out_directory, "assets", "images"))
	//serialization.PackTextures(filepath.Join(in_directory, "assets", "images", "menuImages"), filepath.Join(out_directory, "assets", "images"))
	//serialization.PackTextures(filepath.Join(in_directory, "assets", "images", "menuTitleImages"), filepath.Join(out_directory, "assets", "images"))
}

func copyFiles(in_directory, out_directory string) {
	files := copylist.GetCopyFiles()

	for _, file := range files {
		log.Println("Copying file: " + file)
		in_path := filepath.Join(in_directory, file)
		out_path := filepath.Join(out_directory, file)
		filepath.Dir(out_path)
		log.Println(in_path + " -> " + out_path)

		os.MkdirAll(filepath.Dir(out_path), os.ModePerm)

		bytesRead, err := ioutil.ReadFile(in_path)

		if err != nil {
			log.Fatal(err)
		}

		err = ioutil.WriteFile(out_path, bytesRead, 0644)

		if err != nil {
			log.Fatal(err)
		}
	}
}

func makeList(in_directory string) {
	err := filepath.Walk(in_directory,
		func(path string, info os.FileInfo, err error) error {
			if err != nil {
				return err
			}
			if !info.IsDir() {

				fmt.Println("\"" + strings.Replace(path, "../../src/", "", 1) + "\",")
			}

			return nil
		})
	if err != nil {
		log.Println(err)
	}
}
