#!/usr/bin/env python3
"""Conservative source audit of this repository's GL backend, not driver certification."""
import argparse
from pathlib import Path
import re
import subprocess
import sys
import tempfile

# Deliberately explicit: new entry points require review even when they are <= 3.0.
# Includes core 1.1 imports as well as dynamically loaded core 2.0/3.0 calls.
ALLOWED_GL = set("""
glActiveTexture glGenBuffers glDeleteBuffers glBindBuffer glBufferData
glBufferSubData glMapBufferRange glUnmapBuffer glGenVertexArrays
glDeleteVertexArrays glBindVertexArray glEnableVertexAttribArray
glDisableVertexAttribArray glVertexAttribPointer glVertexAttribIPointer
glCreateShader glShaderSource glCompileShader glGetShaderiv glGetShaderInfoLog
glDeleteShader glCreateProgram glAttachShader glDetachShader glBindAttribLocation
glBindFragDataLocation glLinkProgram glGetProgramiv glGetProgramInfoLog glUseProgram
glDeleteProgram glGetUniformLocation glUniform1i glUniform1f glUniform2f glUniform3f
glUniform4f glUniform4fv glUniformMatrix4fv glUniform1ui glUniform2i glUniform4i
glGenFramebuffers glDeleteFramebuffers glBindFramebuffer glFramebufferTexture2D
glCheckFramebufferStatus glBlitFramebuffer glGenerateMipmap glBlendEquation
glBlendFuncSeparate glBlendEquationSeparate glDrawBuffers glClearBufferfv glGetStringi
glGetString glGetIntegerv glGetFloatv glGetBooleanv glGetError glFinish glFlush
glGenTextures glDeleteTextures glBindTexture glTexImage2D glTexSubImage2D
glTexParameteri glTexParameterf glGetTexImage glPixelStorei glReadPixels
glViewport glScissor glEnable glDisable glClear glClearColor glColorMask
glDrawArrays glDrawElements glBlendFunc glDepthMask glDrawBuffer glReadBuffer glClampColor
glCopyTexSubImage2D glGetTexLevelParameteriv glIsEnabled glBindRenderbuffer
glFenceSync glClientWaitSync glDeleteSync
glBindFragDataLocationIndexed
glGenRenderbuffers glDeleteRenderbuffers glRenderbufferStorage glFramebufferRenderbuffer
""".split())
ALLOWED_WGL = set("""
wglGetProcAddress wglCreateContext wglDeleteContext wglMakeCurrent
wglGetCurrentDC wglGetCurrentContext wglCreateContextAttribsARB wglSwapIntervalEXT
wglGetExtensionsStringARB wglGetExtensionsStringEXT
""".split())
FORBIDDEN_GLSL = re.compile(
    r"\b(?:layout\s*\(|imageLoad\s*\(|imageStore\s*\(|"
    r"imageAtomic\w*\s*\(|memoryBarrier\w*\s*\(|barrier\s*\(|"
    r"sampler2DArrayShadow|samplerBuffer|[iu]?image[123]D|buffer\s+\w+\s*\{)")
CALL = re.compile(r"\b(?:gl[A-Z]\w*|wgl[A-Z]\w*)\b")
SHADER = re.compile(r'R"(?P<tag>\w*)\((?P<source>.*?)\)(?P=tag)"', re.S)


def strip_comments(text):
    # Keep line numbers, strings and raw shader literals for useful diagnostics.
    token = re.compile(r'R"(?P<tag>\w*)\(.*?\)(?P=tag)"|"(?:\\.|[^"\\])*"|'
                       r"'(?:\\.|[^'\\])*'|//[^\n]*|/\*.*?\*/", re.S)
    return token.sub(lambda m: re.sub(r"[^\n]", " ", m[0])
                     if m[0].startswith(("//", "/*")) else m[0], text)


def audit(root):
    backend = root / "pvzPerf/SexyAppFramework/graphics/opengl"
    if not backend.is_dir():
        raise ValueError(f"GL backend directory not found: {backend}")
    files = sorted(p for p in backend.rglob("*")
                   if p.suffix.lower() in {".h", ".hpp", ".cpp", ".cc", ".glsl", ".vert", ".frag", ".comp"})
    shared = backend.parent / "gl"
    files += sorted(p for p in shared.rglob("*")
                    if p.suffix.lower() in {".h", ".hpp", ".cpp"})
    if not files:
        raise ValueError("No GL backend sources found")
    issues = []
    for path in files:
        text = strip_comments(path.read_text(encoding="utf-8-sig"))
        relative = path.relative_to(root)

        def report(offset, message):
            issues.append(f"{relative}:{text.count(chr(10), 0, offset)+1}: {message}")

        # Also catches declarations/function-table macro entries and quoted loader names.
        for match in CALL.finditer(text):
            name = match[0].removesuffix("Proc")
            allowed = ALLOWED_WGL if name.startswith("wgl") else ALLOWED_GL
            if name not in allowed:
                report(match.start(), f"unreviewed GL entry point: {name}")
        if path.name == "OpenGLApi.h":
            required = text.split("#define SEXY_GL3_FUNCTIONS(X)", 1)[1].split("#define SEXY_GL3_DECLARE", 1)[0]
            for optional in ("glFenceSync", "glClientWaitSync", "glDeleteSync", "glBindFragDataLocationIndexed"):
                if optional in required:
                    report(0, f"optional extension entered the required GL 3.0 table: {optional}")
        for match in re.finditer(r"^\s*#\s*version\s+(\d+)[^\n]*", text, re.M):
            if match[1] != "130":
                report(match.start(), f"shader version must be 130, found {match[1]}")
        for match in re.finditer(r"^\s*#\s*extension\s+.*", text, re.M):
            report(match.start(), "GLSL extension directive requires explicit compatibility review")
        for match in SHADER.finditer(text):
            source = match["source"]
            if "#version" not in source:
                continue
            for suspicious in FORBIDDEN_GLSL.finditer(source):
                report(match.start("source")+suspicious.start(),
                       f"suspicious GLSL feature: {suspicious[0]}")
        if path.suffix.lower() in {".glsl", ".vert", ".frag", ".comp"}:
            if not re.search(r"^\s*#\s*version\s+130\b", text, re.M):
                report(0, "standalone shader has no GLSL 130 declaration")
            for suspicious in FORBIDDEN_GLSL.finditer(text):
                report(suspicious.start(), f"suspicious GLSL feature: {suspicious[0]}")
    return len(files), issues


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--glslang", type=Path,
                        help="Also compile every embedded shader with strict GLSL 130")
    args = parser.parse_args()
    try:
        count, issues = audit(args.root.resolve())
        if args.glslang and not issues:
            source = (args.root / "pvzPerf/SexyAppFramework/graphics/gl/GLShaders.cpp").read_text()
            compiled = 0
            with tempfile.TemporaryDirectory(prefix="pvz-glsl130-") as directory:
                for shader in SHADER.finditer(source):
                    text = shader["source"]
                    if "#version" not in text:
                        continue
                    stage = "vert" if "gl_Position" in text else "frag"
                    path = Path(directory) / f"shader{compiled}.{stage}"
                    path.write_text(text)
                    result = subprocess.run(
                        [str(args.glslang), "-S", stage, "--glsl-version", "130", str(path)],
                        capture_output=True, text=True)
                    if result.returncode:
                        raise ValueError(result.stdout + result.stderr)
                    compiled += 1
            if compiled != 6:
                raise ValueError(f"Expected six fixed shader modules, found {compiled}")
            print(f"Strict GLSL 130 compiler validation: {compiled} shaders passed.")
    except (OSError, UnicodeError, ValueError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 2
    for issue in issues:
        print(issue)
    print(f"Scanned {count} GL backend files; suspicious findings={len(issues)}.")
    print("Source text audit only: no shader execution, import audit, Win7 or old-GPU certification.")
    return 1 if issues else 0


if __name__ == "__main__":
    sys.exit(main())
