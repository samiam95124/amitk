"""Turn doc/remote.tex into a chapter of doc/ami.odt.

The paper is a small, known subset of LaTeX -- sections, texttt, tabular,
one description list and one longtable -- so this reads that subset rather
than pretending to be a converter.
"""
import re, zipfile, shutil, sys

TEX  = "doc/remote.tex"
CAT  = "doc/remote_catalog.tex"
ODT  = "doc/ami.odt"
CHAPTER = "The remote display protocol"

# ---------------------------------------------------------------- inline text

def esc(s):
    return s.replace("&","&amp;").replace("<","&lt;").replace(">","&gt;")

def inline(s):
    """LaTeX inline markup to ODF runs.

    The literal stretches between the texttt runs are converted whole, not a
    character at a time: the quotes and the arrows are several characters
    long and would never match otherwise."""
    out = []
    lit = []
    i = 0
    def flush():
        if lit:
            out.append(esc(plain("".join(lit))))
            lit.clear()
    while i < len(s):
        m = re.compile(r'\\texttt\{').match(s, i)
        if m:
            depth, j = 1, m.end()
            while depth:
                if s[j] == '{': depth += 1
                elif s[j] == '}': depth -= 1
                j += 1
            flush()
            out.append('<text:span text:style-name="Reference_20_Char">'
                       + esc(plain(s[m.end():j-1])) + '</text:span>')
            i = j
            continue
        lit.append(s[i])
        i += 1
    flush()
    return "".join(out)

def plain(s):
    """the escapes and the maths, which are only arrows and small numbers"""
    s = s.replace(r"$\rightarrow$", "\u2192")
    s = re.sub(r'\$([^$]*)\$', r'\1', s)
    s = s.replace(r"\_", "_").replace(r"\&", "&").replace(r"\%", "%")
    s = s.replace(r"\{", "{").replace(r"\}", "}").replace(r"\#", "#")
    s = re.sub(r"``([^']*)''", "\u201c\\1\u201d", s)
    s = re.sub(r'(?<!-)--(?!-)', "\u2013", s)   # the en dash, meaning no payload
    s = s.replace(r"\\", "")
    return s

# ------------------------------------------------------------------ ODF parts

P    = '<text:p text:style-name="Standard">%s</text:p>'
PEMP = '<text:p text:style-name="Standard"/>'

def head2(title):
    return ('<text:list text:continue-numbering="true" text:style-name="WWNum3">'
            '<text:list-item><text:list><text:list-item>'
            '<text:h text:style-name="P40" text:outline-level="2">%s</text:h>'
            '</text:list-item></text:list></text:list-item></text:list>'
            % esc(plain(title)))

def table(name, cols, rows, head=None, spans=None):
    """rows: list of lists of already-inlined cells"""
    x = ['<table:table table:name="%s" table:style-name="%s">' % (name, name)]
    for c in "ABCDE"[:cols]:
        x.append('<table:table-column table:style-name="%s.%s"/>' % (name, c))
    def cell(txt, style, para, span=1):
        sp = ' table:number-columns-spanned="%d"' % span if span > 1 else ''
        s = ('<table:table-cell table:style-name="%s.%s" office:value-type="string"%s>'
             '<text:p text:style-name="%s">%s</text:p></table:table-cell>'
             % (name, style, sp, para, txt))
        if span > 1:
            s += '<table:covered-table-cell/>' * (span-1)
        return s
    if head:
        x.append('<table:table-row table:style-name="%s.1">' % name)
        for h in head: x.append(cell(h, "A1", "Table_20_Heading"))
        x.append('</table:table-row>')
    for r in rows:
        x.append('<table:table-row table:style-name="%s.1">' % name)
        if len(r) == 1 and cols > 1:      # a divider row across the table
            x.append(cell(r[0], "A1", "Table_20_Heading", cols))
        else:
            for c in r: x.append(cell(c, "A2", "Table_20_Contents"))
        x.append('</table:table-row>')
    x.append('</table:table>')
    return "".join(x)

def tablestyles(name, widths):
    """cloned from the ports table already in the chapter before this one"""
    total = sum(widths)
    s = ['<style:style style:name="%s" style:family="table">'
         '<style:table-properties style:width="6.4146in" style:rel-width="100%%" '
         'fo:margin-left="-0.075in" fo:margin-top="0in" fo:margin-bottom="0in" '
         'table:align="left" style:writing-mode="lr-tb"/></style:style>' % name]
    for c, w in zip("ABCDE", widths):
        s.append('<style:style style:name="%s.%s" style:family="table-column">'
                 '<style:table-column-properties style:column-width="%.4fin" '
                 'style:rel-column-width="%d*"/></style:style>'
                 % (name, c, w, int(w/total*65535)))
    s.append('<style:style style:name="%s.1" style:family="table-row">'
             '<style:table-row-properties fo:keep-together="auto"/></style:style>' % name)
    for cs, bg in (("A1", ' fo:background-color="#4bacc6"'), ("A2", "")):
        s.append('<style:style style:name="%s.%s" style:family="table-cell">'
                 '<style:table-cell-properties%s fo:padding-left="0.075in" '
                 'fo:padding-right="0.075in" fo:padding-top="0in" '
                 'fo:padding-bottom="0in" fo:border-left="none" fo:border-right="none" '
                 'fo:border-top="%s" fo:border-bottom="%s" style:writing-mode="lr-tb">'
                 '<style:background-image/></style:table-cell-properties></style:style>'
                 % (name, cs, bg,
                    "2.25pt solid #000000" if cs == "A1" else "none",
                    "2.25pt solid #000000" if cs == "A1" else "0.5pt solid #808080"))
    return "".join(s)

# ------------------------------------------------------------------ the paper

tex = open(TEX).read()
body = tex[tex.index(r"\begin{abstract}"):tex.index(r"\end{document}")]

# the abstract opens the chapter, with "paper" made "chapter"
abstract = body[body.index(r"\begin{abstract}")+len(r"\begin{abstract}"):
                body.index(r"\end{abstract}")].strip()
abstract = abstract.replace("This paper", "This chapter").replace("this paper", "this chapter")
abstract = abstract.replace("The protocol is defined", "The protocol is defined")

out = [P % inline(" ".join(abstract.split()))]

rest = body[body.index(r"\section{Introduction}"):]
# drop the longtable scaffolding; the catalog is built from its own file
rest = re.sub(r'\\begin\{longtable\}.*?\\endhead', '@CATALOG@', rest, flags=re.S)
rest = re.sub(r'(?:\\hline\s*)?\\input\{remote_catalog\}\s*\\hline\s*\\end\{longtable\}', '', rest, flags=re.S)

tabno = 0
tabstyles = []

def smalltable(txt):
    """a center+tabular: rows of & separated cells"""
    global tabno
    tabno += 1
    name = "TableProto%d" % tabno
    rows = []
    for raw in txt.split(r"\\"):
        raw = raw.strip()
        if not raw: continue
        rows.append([inline(" ".join(c.split())) for c in raw.split("&")])
    cols = max(len(r) for r in rows)
    for r in rows: r += [""] * (cols - len(r))
    widths = {2: [1.3, 5.1], 3: [1.0, 1.5, 3.9]}[cols]
    tabstyles.append(tablestyles(name, widths))
    return table(name, cols, rows)

blocks = re.split(r'(\\section\{[^}]*\}|\\subsection\{[^}]*\}|'
                  r'\\begin\{center\}.*?\\end\{center\}|'
                  r'\\begin\{description\}.*?\\end\{description\}|@CATALOG@)',
                  rest, flags=re.S)
for b in blocks:
    b = b.strip()
    if not b: continue
    m = re.match(r'\\(sub)?section\{([^}]*)\}$', b)
    if m:
        out.append(head2(m.group(2)))
        continue
    if b.startswith(r"\begin{center}"):
        inner = b[b.index(r"\begin{tabular}"):b.index(r"\end{tabular}")]
        inner = inner[inner.index("}", inner.index("{", inner.index("tabular"))+1)+1:]
        out.append(smalltable(inner))
        continue
    if b.startswith(r"\begin{description}"):
        for item in re.findall(r'\\item\[([^\]]*)\](.*?)(?=\\item\[|\\end\{description\})', b, re.S):
            term, txt = item
            out.append('<text:p text:style-name="Standard">%s<text:line-break/>%s</text:p>'
                       % (inline(term), inline(" ".join(txt.split()))))
        continue
    if b == "@CATALOG@":
        out.append("@CATALOG@")
        continue
    for para in re.split(r'\n\s*\n', b):
        para = " ".join(para.split())
        if para: out.append(P % inline(para))

# --------------------------------------------------------------- the catalog

rows = []
for line in open(CAT):
    line = line.strip()
    if not line or line.startswith("%") or line == r"\hline": continue
    mc = re.match(r'\\multicolumn.*?\\textbf\{(.*)\}\}\s*\\\\$', line)
    if mc:
        rows.append([inline(mc.group(1))]); continue
    line = re.sub(r'\\\\$', '', line).strip()
    cells = [c.strip() for c in line.split("&")]
    if len(cells) != 3:
        sys.exit("catalog row not three cells: " + line[:70])
    rows.append([inline(c) for c in cells])
tabstyles.append(tablestyles("TableProtoCat", [2.1, 0.6, 3.7]))
catalog = table("TableProtoCat", 3, rows,
                head=["Message", "Id", "Payload \u2192 reply"])
out = [catalog if o == "@CATALOG@" else o for o in out]
print("catalog rows:", len(rows))

chapter = "".join(out)

# ------------------------------------------------------- put it in the manual

zin = zipfile.ZipFile(ODT)
names = zin.namelist()
c = zin.read("content.xml").decode("utf8")

# the chapter heading, banner and all, on the pattern of the one before it
CAT_IMG = "Pictures/10000001000003400000035C2E5150E9.png"
# The chapter numbers by hanging off the list of the chapter before it. That
# list's id is read here and not written down: LibreOffice renames every list
# in the document whenever the indexes are refreshed, so an id copied into
# this script would be dangling the first time that happened, and the chapter
# would number itself 1.
prevch = c.rfind("<text:list ", 0, c.rfind("<text:h", 0, c.index('draw:name="ChapCatRemote"')))
tag = c[prevch:c.index(">", prevch)]
m = re.search(r'xml:id="([^"]+)"', tag) or re.search(r'text:continue-list="([^"]+)"', tag)
if m:
    hang = 'text:continue-list="' + m.group(1) + '"'
elif 'text:continue-numbering="true"' in tag:
    # LibreOffice also writes chapters that simply continue the numbering of
    # whatever list came before them, with no id at all; this one does the same
    hang = 'text:continue-numbering="true"'
else:
    raise SystemExit("the chapter before this one hangs off no list: " + tag[:80])

heading = ('<text:list ' + hang + ' text:style-name="WWNum3">'
           '<text:list-item>'
           '<text:h text:style-name="Chapter_20_Banner" text:outline-level="1" '
           'loext:marker-style-name="T20">'
           '<draw:frame text:anchor-type="paragraph" draw:z-index="140" '
           'draw:name="ChapCatProtocol" draw:style-name="gr1" draw:text-style-name="P9" '
           'svg:width="1.7398in" svg:height="1.8004in" svg:x="4.5945in" svg:y="-0.7083in">'
           '<draw:image xlink:href="%s" xlink:type="simple" xlink:show="embed" '
           'xlink:actuate="onLoad" draw:mime-type="image/png"><text:p/></draw:image>'
           '</draw:frame>%s</text:h>'
           '</text:list-item></text:list>' % (CAT_IMG, CHAPTER))

# A chapter already here is replaced, so this can be run again whenever the
# paper or the catalog changes: from the list that opens it to the list that
# opens the chapter after it.
if 'draw:name="ChapCatProtocol"' in c:

    old = c.index('draw:name="ChapCatProtocol"')
    ohd = c.rfind("<text:h", 0, old)
    obeg = c.rfind("<text:list ", 0, ohd)
    onxt = c.index('draw:name="ChapCatWidget"', old)
    ohd2 = c.rfind("<text:h", 0, onxt)
    oend = c.rfind("<text:list ", 0, ohd2)
    assert 0 < obeg < ohd < oend < ohd2
    c = c[:obeg] + c[oend:]
    print("the chapter that was there has been taken out")

# it goes after Remote mode, which means before the chapter that follows it
nxt = c.index('draw:name="ChapCatWidget"')   # the chapter that follows Remote mode
hd  = c.rfind("<text:h", 0, nxt)               # its heading
ins = c.rfind("<text:list ", 0, hd)            # and the list that opens it
assert 0 < ins < hd
assert c[ins:hd].count("<text:h") == 0, "the anchor reaches past a heading"
c = c[:ins] + heading + chapter + c[ins:]

# the table styles go with the rest of the automatic ones
c = c.replace("</office:automatic-styles>", "".join(tabstyles) + "</office:automatic-styles>", 1)

# the chapter before it pointed at the paper; now it points here
old = ("<text:span text:style-name=\"T135\">If you are interested, the protocol "
       "description for this is in doc/remote.pdf.</text:span>")
new = ("<text:span text:style-name=\"T135\">The protocol itself is described in the "
       "next chapter, and in doc/remote.pdf, which is the same text as a paper.</text:span>")
if old in c: c = c.replace(old, new)
else: print("note: the pointer sentence was not where it was expected")

out = ODT + ".new"
zout = zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED)
zout.writestr(zipfile.ZipInfo("mimetype"), zin.read("mimetype"), zipfile.ZIP_STORED)
for n in names:
    if n == "mimetype": continue
    zout.writestr(zin.getinfo(n), c.encode("utf8") if n == "content.xml" else zin.read(n))
zout.close(); zin.close()
shutil.move(out, ODT)
print("chapter added:", CHAPTER)
