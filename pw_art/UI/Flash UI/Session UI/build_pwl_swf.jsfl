// Builds pwl.swf of the loading screen from pwl.fla + classes in Flash CS6 without the UI:
//   "F:\Adobe\Adobe Flash CS6\Flash.exe" "<this folder>\build_pwl_swf.jsfl"
// Result: pwl.swf next to pwl.fla and a copy in pw/branches/r1117/Data/UI/Screens/Loading/Flash/pwl.swf;
// compiler output: build_pwl_swf.log, errors: build_pwl_swf_errors.log. Flash quits when done.
var here = fl.scriptURI.substr(0, fl.scriptURI.lastIndexOf("/") + 1);
var swf = here + "pwl.swf";
var doc = fl.openDocument(here + "pwl.fla");
fl.outputPanel.clear();
doc.exportSWF(swf, true);
fl.outputPanel.save(here + "build_pwl_swf.log");
try { fl.compilerErrors.save(here + "build_pwl_swf_errors.log"); } catch (e) {}
doc.close(false);
// pw_art/UI/Flash UI/Session UI/ -> pw/branches/r1117/Data/UI/Screens/Loading/Flash/
var root = here.substr(0, here.lastIndexOf("/pw_art/") + 1);
var target = root + "pw/branches/r1117/Data/UI/Screens/Loading/Flash/";
FLfile.copy(swf, target + "pwl_new.swf.tmp");
FLfile.remove(target + "pwl.swf");
FLfile.copy(target + "pwl_new.swf.tmp", target + "pwl.swf");
FLfile.remove(target + "pwl_new.swf.tmp");
fl.quit(false);
