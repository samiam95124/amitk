"""Check the chapter banners of doc/ami.odt against the three rules they follow.

Every chapter opens with a banner: the blue box carrying the chapter number,
its title, and the cat. The rules are

  1. every chapter has one,
  2. the title keeps clear of the cat, which sits inside the box, and
  3. the chapter starts on an odd page, so it opens on a right hand leaf.

Rules 1 and 3 hold by themselves as long as the heading carries the Chapter
Banner style: that style names the ChapterStart page, whose layout is right
only, so LibreOffice breaks to the next odd page and inserts a blank leaf when
it has to. Rule 2 holds because the cat's frame wraps text to its left. The
point of this program is to prove all three of a rendered book rather than
trust them.

There is a fourth thing to prove, about the PDF rather than the document. The
blank leaf that puts a chapter on a right hand page is one LibreOffice inserts
for itself, and its PDF export leaves such leaves out unless told otherwise:
the page numbers then run 55, 57, and a book printed two sided from that file
has chapter 4 on the back of page 55 after all. The export here keeps them
(IsSkipEmptyPages off; in the export dialog, "Export automatically inserted
blank pages"), and a PDF handed in is checked for numbers that skip by two,
which is the mark of a leaf dropped on the way out.

Usage:

    python3 doc/checkbanners.py [doc/ami.pdf]

With no argument the book is rendered from doc/ami.odt into a temporary
directory first, which wants soffice. Exits nonzero if any rule is broken.
"""
import os, re, subprocess, sys, tempfile

ODT = "doc/ami.odt"

# The cat's left edge, in points from the left of the page. The text column runs
# from the 1.0425in margin for 6.4146in, and the frame is 1.7398in wide against
# its right edge, which puts the cat's left edge here. A title word reaching
# past it is a title printed over the cat.
CATLEFT = 406.0

# the chapters, in the order they appear
TITLES = [
    "Introduction: What is Ami", "Using C++ With Ami", "System Services Library",
    "Terminal Interface Library", "Character Windows Management Library",
    "Graphical Interface Library", "Graphical Windows Management Library",
    "Widgets Library",
    "Sound Library", "Networking Library", "Option: Command Line Option Processing",
    "Config: The configuration database", "Print modules", "Remote mode",
    "The remote display protocol", "Widget Creation", "Sound module plugins",
    "Example Applications", "Libc and alternatives", "Directory layout",
    "Installing and building Ami", "Building user applications", "Testing Ami", "Windows Specific Details",
    "Linux Specific details", "Mac OS X specific details", "License",
]

def render(tmp):
    """the book as a pdf, from the odt, with the blank leaves in it. The
    export runs in a LibreOffice profile of its own, so a LibreOffice the
    user has open, the document included, is neither disturbed nor relied
    on."""
    export = ('pdf:writer_pdf_Export:'
              '{"IsSkipEmptyPages":{"type":"boolean","value":false}}')
    profile = os.path.join(tmp, "profile")
    subprocess.run(["soffice", "--headless",
                    "-env:UserInstallation=file://" + profile,
                    "--convert-to", export, "--outdir", tmp, ODT], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return os.path.join(tmp, "ami.pdf")

def dropped(nums):
    """the blank leaves the export left out: places where the printed page
    numbers of two pages in a row differ by two. nums carries each page's
    printed number (None for none) and whether the page is a blank leaf."""
    out, prev = [], None
    for n, blank in nums:
        if n is None:
            # a page with nothing on it is a blank leaf that was kept; it
            # carries no number, but it holds the place of one
            if prev is not None and blank: prev += 1
            continue
        if prev is not None and n == prev+2: out.append(prev+1)
        prev = n
    return out

def pages(pdf):
    """the text of each page, and the words of each page with their boxes"""
    txt = subprocess.run(["pdftotext", pdf, "-"], check=True,
                         capture_output=True).stdout.decode("utf8", "replace")
    box = subprocess.run(["pdftotext", "-bbox", pdf, "-"], check=True,
                         capture_output=True).stdout.decode("utf8", "replace")
    words = [re.findall(r'<word xMin="([\d.]+)" yMin="([\d.]+)" '
                        r'xMax="([\d.]+)" yMax="([\d.]+)">([^<]*)</word>', p)
             for p in re.findall(r'<page [^>]*>(.*?)</page>', box, re.S)]
    heights = [float(h) for h in
               re.findall(r'<page width="[\d.]+" height="([\d.]+)"', box)]
    return txt.split("\f"), words, heights

def printed(words, height):
    """the page number the page prints: digits standing alone in the footer,
    the bottom stretch of the page. Position, not text order, so a table of
    contents entry's target number wrapped onto its own line cannot pass
    for the page number."""
    n = [w for a, b, c, d, w in words
         if w.strip().isdigit() and len(w.strip()) <= 4 and float(b) > height-120]
    return int(n[-1].strip()) if n else None

def printsblanks():
    """whether the document itself prints its inserted blank leaves.

    PrintEmptyPages is the document's own switch, saved with it in
    settings.xml and shown as "Print automatically inserted blank pages" under
    Writer's print options. Off, Print Preview and a print from Writer drop
    the leaves, whatever the PDF export does with them."""
    import zipfile
    s = zipfile.ZipFile(ODT).read("settings.xml").decode("utf8")
    m = re.search(r'config:name="PrintEmptyPages"[^>]*>([^<]*)<', s)
    return m is None or m.group(1) == "true"

def main():
    tmp = None
    if len(sys.argv) > 1: pdf = sys.argv[1]
    else:
        tmp = tempfile.mkdtemp()
        pdf = render(tmp)
    text, words, heights = pages(pdf)
    nums = [(printed(words[i], heights[i]), not text[i].strip())
            for i in range(len(words))]
    faults = 0
    if os.path.exists(ODT) and not printsblanks():
        faults += 1
        print("the document has PrintEmptyPages off: Print Preview and a print "
              "from Writer drop the blank leaves, and the chapters after each "
              "one open on a left hand page")
    for n, title in enumerate(TITLES, 1):
        opening = " ".join(title.split()[:2])
        found = [i for i, p in enumerate(text)
                 if re.search(r'(?m)^%d %s' % (n, re.escape(opening)), p)]
        if not found:
            print("%2d  %-40s no chapter page" % (n, title[:40])); faults += 1
            continue
        i = found[-1]
        page = nums[i][0]
        # the banner is the large text on the page
        big = [(float(a), float(b), float(c), float(d), w)
               for a, b, c, d, w in words[i] if float(d)-float(b) > 14]
        if not big:
            print("%2d  %-40s no banner" % (n, title[:40])); faults += 1
            continue
        over = [w for a, b, c, d, w in big if c > CATLEFT]
        note = ""
        if over: note += "  over the cat: " + " ".join(over)
        if page is None: note += "  no page number"
        elif page % 2 == 0: note += "  starts on an even page"
        if note: faults += 1
        print("%2d  %-40s page %-5s right %5.1f%s"
              % (n, title[:40], page, max(x[2] for x in big), note))
    gone = dropped(nums)
    if gone:
        faults += len(gone)
        print()
        print("blank leaves missing from the PDF, at pages %s: export with "
              "the automatically inserted blank pages kept, or a two sided "
              "print puts those chapters on a left hand page"
              % ", ".join(str(g) for g in gone))
    print()
    print("%d chapters, %d faults" % (len(TITLES), faults))
    if tmp: subprocess.run(["rm", "-rf", tmp])
    return 1 if faults else 0

if __name__ == "__main__":
    sys.exit(main())
