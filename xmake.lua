set_languages("cxx17")
set_arch("x86")
option("HL2SDKPATH")

--Manually compile tier1
target("tier1")
	set_kind("static")
	add_files("$(HL2SDKPATH)/tier1/*.cpp")

	remove_files(
		--processor_detect.cpp Auto detection includes.
		"$(HL2SDKPATH)/tier1/processor_detect_linux.cpp",

		-- There is no place to use these functions, and compilation will result in link errors.
		"$(HL2SDKPATH)/tier1/diff.cpp", 
		"$(HL2SDKPATH)/tier1/undiff.cpp"
	)

	add_includedirs(
		"$(HL2SDKPATH)/public",
		"$(HL2SDKPATH)/public/tier0",
		"$(HL2SDKPATH)/public/tier1",
		"$(HL2SDKPATH)/public/engine",
		"$(HL2SDKPATH)/public/mathlib"
	)

	add_defines("NDEBUG")

	if is_plat("windows") then
		set_toolchains("msvc")
		add_defines(
			"_WINDOWS", "WIN32",
			'_CRT_SECURE_NO_WARNINGS')
		add_cxflags("/W3", "/wd4819", "/wd5033", "/Ox", "/Oy-", "/EHsc", "/MT", "/Z7")

	else
		set_toolchains("clang")
		add_defines(
			"_LINUX","POSIX",
			"NO_HOOK_MALLOC", "NO_MALLOC_OVERRIDE",
			"stricmp=strcasecmp",
			"_stricmp=strcasecmp",
			"strcmpi=strcasecmp",
			"_strnicmp=strncasecmp",
			"strnicmp=strncasecmp ",
			"_snprintf=snprintf",
			"_vsnprintf=vsnprintf",
			"_alloca=alloca"
		)
		add_cxflags(
			"-Wall", 
			"-Wshadow",
			"-Wno-parentheses-equality",
			"-Wno-undefined-bool-conversion",
			"-Wno-implicit-int-float-conversion", 
			"-Wno-overloaded-virtual", 
			"-Wno-deprecated-register", 
			"-Wno-register",
			"-Wno-non-virtual-dtor", 
			"-Wno-expansion-to-defined",
			"-fno-strict-aliasing", 
			"-fno-exceptions", 
			"-fvisibility=hidden", 
			"-fvisibility-inlines-hidden", 
			"-flto", "-fPIC", "-O2", "-g3"
		)
	end


target("l4dtoolz")
	set_kind("shared")
	add_deps("tier1")
	set_prefixname("")
	
	add_files("src/*.cpp")

	add_includedirs(
		"src",
		"$(HL2SDKPATH)/public",
		"$(HL2SDKPATH)/public/tier0",
		"$(HL2SDKPATH)/public/tier1",
		"$(HL2SDKPATH)/public/engine",
		"$(HL2SDKPATH)/public/mathlib"
	)

	add_defines("NDEBUG")

	if is_plat("windows") then
		set_toolchains("msvc")
		add_defines(
			"_WINDOWS", "WIN32",
			"strdup=_strdup",
			"_CRT_SECURE_NO_WARNINGS")
		add_cxflags("/W3", "/wd4819", "/Ox", "/Oy-", "/EHsc", "/MT", "/Z7")

		add_shflags("/DEBUG") --Always generate pdb files
		add_shflags("/OPT:ICF", "/OPT:REF")
		add_linkdirs("$(HL2SDKPATH)/lib/public");
		add_links("tier0", "vstdlib", "kernel32", "legacy_stdio_definitions")
		
	else
		set_toolchains("clang")
		add_defines(
			"_LINUX","POSIX",
			"NO_HOOK_MALLOC", "NO_MALLOC_OVERRIDE",
			"stricmp=strcasecmp",
			"_stricmp=strcasecmp",
			"strcmpi=strcasecmp",
			"_strnicmp=strncasecmp",
			"strnicmp=strncasecmp ",
			"_snprintf=snprintf",
			"_vsnprintf=vsnprintf",
			"_alloca=alloca"
		)
		add_cxflags(
			"-Wall", 
			"-Wshadow",
			"-Wno-implicit-int-float-conversion", 
			"-Wno-overloaded-virtual", 
			"-Wno-deprecated-register", 
			"-Wno-register",
			"-Wno-non-virtual-dtor", 
			"-Wno-expansion-to-defined",
			"-fno-strict-aliasing", 
			"-fno-exceptions", 
			"-fvisibility=hidden", 
			"-fvisibility-inlines-hidden", 
			"-flto", "-fPIC", "-O2", "-g3"
		)

		add_linkdirs("$(HL2SDKPATH)/lib/linux");
		add_links("tier0_srv", "vstdlib_srv")
		add_shflags("-fuse-ld=lld", "-flto", "-static-libstdc++", "-static-libgcc")
	end
	
	after_build(function (target)
		os.tryrm("release")
		os.mkdir("release/addons/l4dtoolz")
		os.cp(path.join(target:targetdir(), target:filename()), "release/addons/l4dtoolz")
        os.cp("extra/l4dtoolz.txt", "release/addons/l4dtoolz")
        os.cp("extra/l4dtoolz.vdf", "release/addons")
    end)

