// Imports the halloween skin bitmaps art/halloween/final/HW_*.png into main.fla (library folder "halloween"),
// each exported for AS as a BitmapData class with the file name (HW_Wood, HW_FrameTL...), and saves main.fla.
//   "D:\AdobeFlashCS6\Adobe Flash CS6\Flash.exe" "<this folder>\import_halloween.jsfl"
// then build main.swf with build_main_swf.jsfl. Re-running replaces the bitmaps. Log: import_halloween.log
var here = fl.scriptURI.substr(0, fl.scriptURI.lastIndexOf("/") + 1);
var src = here + "art/halloween/final/";
var folder = "halloween";
var log = [];
var doc = fl.openDocument(here + "main.fla");
var lib = doc.library;
if (!lib.itemExists(folder))
	lib.newFolder(folder);
var files = FLfile.listFolder(src + "*.png", "files");
for (var i = 0; i < files.length; i++)
{
	var file = files[i];
	var cls = file.substr(0, file.length - 4);
	var path = folder + "/" + file;
	if (lib.itemExists(path))
		lib.deleteItem(path);
	if (lib.itemExists(file))
		lib.deleteItem(file);
	doc.importFile(src + file, true);
	if (!lib.itemExists(file))
	{
		log.push("FAIL import " + file);
		continue;
	}
	lib.moveToFolder(folder, file, true);
	lib.selectItem(path);
	var item = lib.getSelectedItems()[0];
	item.allowSmoothing = true;
	// only lossless: the game's Flash player has no DefineBitsJPEG2 ("photo" compression asserts on load)
	item.compressionType = "lossless";
	item.linkageExportForAS = true;
	item.linkageExportInFirstFrame = true;
	item.linkageBaseClass = "flash.display.BitmapData";
	item.linkageClassName = cls;
	log.push("ok " + path + " -> " + cls);
}
doc.save();
doc.close(false);
FLfile.write(here + "import_halloween.log", log.join("\r\n") + "\r\n");
fl.quit(false);
