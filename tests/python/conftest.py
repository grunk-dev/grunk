from grunk.codegen import has_clang

collect_ignore = []
if not has_clang():
    collect_ignore.append("test_codegen.py")