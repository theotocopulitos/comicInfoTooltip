// ComicInfo.cpp : ComicInfo.xml parsing implementation

#include "pch.h"
#include "ComicInfo.h"

ComicInfoParser::ComicInfoParser()  { Clear(); }
ComicInfoParser::~ComicInfoParser() { Clear(); }

bool ComicInfoParser::ParseFromXML(const std::wstring& xmlContent)
{
    Clear();
    if (xmlContent.empty()) return false;
    return ParseXMLElement(xmlContent);
}

void ComicInfoParser::Clear()
{
    m_data = ComicInfoData();
}

// -----------------------------------------------------------------------
// Plain-text formatter (used by IQueryInfo tooltip)
// -----------------------------------------------------------------------

static const wchar_t* kSep  = L"\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500";

static void Line(std::wstring& out, const wchar_t* label, const std::wstring& value)
{
    if (!value.empty())
    {
        out += label;
        out += value;
        out += L"\r\n";
    }
}

static void WrapAppend(std::wstring& out, const std::wstring& text, size_t maxLen = 72)
{
    if (text.empty()) return;
    size_t pos = 0;
    while (pos < text.size())
    {
        // Try to break at a word boundary near maxLen
        size_t end = pos + maxLen;
        if (end >= text.size())
        {
            out += text.substr(pos);
            out += L"\r\n";
            break;
        }
        // Find last space before end
        size_t brk = text.rfind(L' ', end);
        if (brk == std::wstring::npos || brk <= pos)
            brk = end;
        out += text.substr(pos, brk - pos);
        out += L"\r\n";
        pos = brk + 1;
    }
}

// Build a date string from Year/Month/Day fields
static std::wstring BuildDateStr(const std::wstring& y, const std::wstring& m, const std::wstring& d)
{
    if (y.empty() || y == L"-1") return L"";
    std::wstring dt = y;
    if (!m.empty() && m != L"-1")
    {
        if (m.length() == 1) dt += L"-0"; else dt += L"-";
        dt += m;
        if (!d.empty() && d != L"-1")
        {
            if (d.length() == 1) dt += L"-0"; else dt += L"-";
            dt += d;
        }
    }
    return dt;
}

// Build star display  e.g. "★★★★☆ 4.0"
static std::wstring BuildRating(const std::wstring& val)
{
    if (val.empty()) return L"";
    double r = 0;
    try { r = std::stod(val); } catch (...) { return L""; }
    if (r < 0 || r > 5) return L"";
    int full = (int)r;
    std::wstring s;
    for (int i = 0; i < 5; i++)
        s += (i < full) ? L"\u2605" : L"\u2606";   // ★ / ☆
    wchar_t buf[16];
    _snwprintf_s(buf, _countof(buf), _TRUNCATE, L" %.1f", r);
    s += buf;
    return s;
}

// Filter "Unknown" and "-1" defaults from schema
static bool IsSet(const std::wstring& v)
{
    return !v.empty() && v != L"Unknown" && v != L"-1" && v != L"0";
}

std::wstring ComicInfoParser::FormatAsText() const
{
    std::wstring t;

    // ═══════════════════════════════════════════════════════════════════════
    // TITLE BLOCK
    // ═══════════════════════════════════════════════════════════════════════
    std::wstring titleLine;
    if (!m_data.Series.empty())
    {
        titleLine = m_data.Series;
        if (!m_data.Volume.empty() && m_data.Volume != L"-1")
        {
            titleLine += L" \u2022 Vol. "; titleLine += m_data.Volume;
        }
        if (!m_data.Number.empty())
        {
            titleLine += L" #"; titleLine += m_data.Number;
            if (!m_data.Count.empty() && m_data.Count != L"-1")
            {
                titleLine += L" of "; titleLine += m_data.Count;
            }
        }
    }
    else if (!m_data.Title.empty())
    {
        titleLine = m_data.Title;
    }

    if (!titleLine.empty()) { t += titleLine; t += L"\r\n"; }

    // Title as subtitle when Series is the primary heading
    if (!m_data.Series.empty() && !m_data.Title.empty())
    {
        t += L"  \u201c"; t += m_data.Title; t += L"\u201d\r\n";
    }

    // Alternate series
    if (!m_data.AlternateSeries.empty())
    {
        t += L"  (";
        t += m_data.AlternateSeries;
        if (!m_data.AlternateNumber.empty()) { t += L" #"; t += m_data.AlternateNumber; }
        if (!m_data.AlternateCount.empty() && m_data.AlternateCount != L"-1")
        {
            t += L" of "; t += m_data.AlternateCount;
        }
        t += L")\r\n";
    }

    // Community rating right below title
    std::wstring rating = BuildRating(m_data.CommunityRating);
    if (!rating.empty()) { t += L"  "; t += rating; t += L"\r\n"; }

    // ═══════════════════════════════════════════════════════════════════════
    // PUBLICATION
    // ═══════════════════════════════════════════════════════════════════════
    std::wstring dateStr = BuildDateStr(m_data.Year, m_data.Month, m_data.Day);

    bool hasPub = !m_data.Publisher.empty() || !dateStr.empty()
               || !m_data.Imprint.empty()   || !m_data.Genre.empty()
               || !m_data.Tags.empty()      || !m_data.Format.empty()
               || !m_data.LanguageISO.empty() || IsSet(m_data.AgeRating)
               || IsSet(m_data.PageCount)   || IsSet(m_data.BlackAndWhite)
               || IsSet(m_data.Manga)       || !m_data.GTIN.empty()
               || !m_data.Web.empty();
    if (hasPub)
    {
        t += kSep; t += L"\r\n";

        // Publisher + Imprint + date on one smart line
        if (!m_data.Publisher.empty())
        {
            t += L"\u2022 "; t += m_data.Publisher;
            if (!m_data.Imprint.empty()) { t += L" / "; t += m_data.Imprint; }
            if (!dateStr.empty()) { t += L"  ("; t += dateStr; t += L")"; }
            t += L"\r\n";
        }
        else
        {
            Line(t, L"\u2022 Imprint:     ", m_data.Imprint);
            Line(t, L"\u2022 Date:        ", dateStr);
        }

        Line(t, L"\u2022 Genre:       ", m_data.Genre);
        Line(t, L"\u2022 Tags:        ", m_data.Tags);
        Line(t, L"\u2022 Format:      ", m_data.Format);
        Line(t, L"\u2022 Language:    ", m_data.LanguageISO);
        if (IsSet(m_data.AgeRating))
            Line(t, L"\u2022 Age Rating:  ", m_data.AgeRating);
        if (IsSet(m_data.PageCount))
            Line(t, L"\u2022 Pages:       ", m_data.PageCount);

        // Flags: B&W, Manga
        std::wstring flags;
        if (m_data.BlackAndWhite == L"Yes") flags += L"Black & White";
        if (m_data.Manga == L"Yes") { if (!flags.empty()) flags += L"  \u2022  "; flags += L"Manga"; }
        if (m_data.Manga == L"YesAndRightToLeft") { if (!flags.empty()) flags += L"  \u2022  "; flags += L"Manga (RTL)"; }
        if (!flags.empty()) Line(t, L"\u2022 Flags:       ", flags);

        Line(t, L"\u2022 ISBN/GTIN:   ", m_data.GTIN);
        Line(t, L"\u2022 Web:         ", m_data.Web);
    }

    // ═══════════════════════════════════════════════════════════════════════
    // CREATIVE TEAM
    // ═══════════════════════════════════════════════════════════════════════
    bool hasCrew = !m_data.Writer.empty()      || !m_data.Penciller.empty()
                || !m_data.Inker.empty()        || !m_data.Colorist.empty()
                || !m_data.Letterer.empty()     || !m_data.CoverArtist.empty()
                || !m_data.Editor.empty()       || !m_data.Translator.empty();
    if (hasCrew)
    {
        t += kSep; t += L"\r\n";
        Line(t, L"\u270f Writer:      ", m_data.Writer);
        Line(t, L"\u270f Penciller:   ", m_data.Penciller);
        Line(t, L"\u270f Inker:       ", m_data.Inker);
        Line(t, L"\u270f Colorist:    ", m_data.Colorist);
        Line(t, L"\u270f Letterer:    ", m_data.Letterer);
        Line(t, L"\u270f Cover Art:   ", m_data.CoverArtist);
        Line(t, L"\u270f Editor:      ", m_data.Editor);
        Line(t, L"\u270f Translator:  ", m_data.Translator);
    }

    // ═══════════════════════════════════════════════════════════════════════
    // STORY / CHARACTERS
    // ═══════════════════════════════════════════════════════════════════════
    bool hasStory = !m_data.StoryArc.empty()   || !m_data.Characters.empty()
                 || !m_data.Teams.empty()       || !m_data.Locations.empty()
                 || !m_data.SeriesGroup.empty() || !m_data.MainCharacterOrTeam.empty();
    if (hasStory)
    {
        t += kSep; t += L"\r\n";
        Line(t, L"\u2605 Main:        ", m_data.MainCharacterOrTeam);
        if (!m_data.StoryArc.empty())
        {
            std::wstring arc = m_data.StoryArc;
            if (!m_data.StoryArcNumber.empty()) { arc += L" (Part "; arc += m_data.StoryArcNumber; arc += L")"; }
            Line(t, L"\u2605 Story Arc:   ", arc);
        }
        Line(t, L"\u2605 Characters:  ", m_data.Characters);
        Line(t, L"\u2605 Teams:       ", m_data.Teams);
        Line(t, L"\u2605 Locations:   ", m_data.Locations);
        Line(t, L"\u2605 Series Group:", m_data.SeriesGroup);
    }

    // ═══════════════════════════════════════════════════════════════════════
    // SUMMARY
    // ═══════════════════════════════════════════════════════════════════════
    if (!m_data.Summary.empty())
    {
        t += kSep; t += L"\r\n";
        WrapAppend(t, m_data.Summary, 72);
    }

    // ═══════════════════════════════════════════════════════════════════════
    // NOTES / REVIEW
    // ═══════════════════════════════════════════════════════════════════════
    if (!m_data.Notes.empty())
    {
        t += kSep; t += L"\r\n";
        t += L"\u2139 Notes: ";
        WrapAppend(t, m_data.Notes, 64);
    }
    if (!m_data.Review.empty())
    {
        t += kSep; t += L"\r\n";
        t += L"\u2139 Review: ";
        WrapAppend(t, m_data.Review, 63);
    }

    // ═══════════════════════════════════════════════════════════════════════
    // SCAN INFO
    // ═══════════════════════════════════════════════════════════════════════
    if (!m_data.ScanInformation.empty())
    {
        t += kSep; t += L"\r\n";
        Line(t, L"\u2139 Scan: ", m_data.ScanInformation);
    }

    // Remove trailing CRLF
    while (t.size() >= 2 && t.substr(t.size() - 2) == L"\r\n")
        t.resize(t.size() - 2);

    return t;
}

// -----------------------------------------------------------------------
// XML parsing
// -----------------------------------------------------------------------

bool ComicInfoParser::ParseXMLElement(const std::wstring& xml)
{
    // ── Identity ──
    m_data.Title              = ExtractXMLValue(xml, L"Title");
    m_data.Series             = ExtractXMLValue(xml, L"Series");
    m_data.Number             = ExtractXMLValue(xml, L"Number");
    m_data.Count              = ExtractXMLValue(xml, L"Count");
    m_data.Volume             = ExtractXMLValue(xml, L"Volume");

    // ── Alternate series ──
    m_data.AlternateSeries    = ExtractXMLValue(xml, L"AlternateSeries");
    m_data.AlternateNumber    = ExtractXMLValue(xml, L"AlternateNumber");
    m_data.AlternateCount     = ExtractXMLValue(xml, L"AlternateCount");

    // ── Text blocks ──
    m_data.Summary            = ExtractXMLValue(xml, L"Summary");
    m_data.Notes              = ExtractXMLValue(xml, L"Notes");
    m_data.Review             = ExtractXMLValue(xml, L"Review");

    // ── Date ──
    m_data.Year               = ExtractXMLValue(xml, L"Year");
    m_data.Month              = ExtractXMLValue(xml, L"Month");
    m_data.Day                = ExtractXMLValue(xml, L"Day");

    // ── Creative team ──
    m_data.Writer             = ExtractXMLValue(xml, L"Writer");
    m_data.Penciller          = ExtractXMLValue(xml, L"Penciller");
    m_data.Inker              = ExtractXMLValue(xml, L"Inker");
    m_data.Colorist           = ExtractXMLValue(xml, L"Colorist");
    m_data.Letterer           = ExtractXMLValue(xml, L"Letterer");
    m_data.CoverArtist        = ExtractXMLValue(xml, L"CoverArtist");
    m_data.Editor             = ExtractXMLValue(xml, L"Editor");
    m_data.Translator         = ExtractXMLValue(xml, L"Translator");

    // ── Publishing ──
    m_data.Publisher          = ExtractXMLValue(xml, L"Publisher");
    m_data.Imprint            = ExtractXMLValue(xml, L"Imprint");
    m_data.Genre              = ExtractXMLValue(xml, L"Genre");
    m_data.Tags               = ExtractXMLValue(xml, L"Tags");
    m_data.Web                = ExtractXMLValue(xml, L"Web");
    m_data.Format             = ExtractXMLValue(xml, L"Format");
    m_data.GTIN               = ExtractXMLValue(xml, L"GTIN");

    // ── Numeric / enum ──
    m_data.PageCount          = ExtractXMLValue(xml, L"PageCount");
    m_data.LanguageISO        = ExtractXMLValue(xml, L"LanguageISO");
    m_data.BlackAndWhite      = ExtractXMLValue(xml, L"BlackAndWhite");
    m_data.Manga              = ExtractXMLValue(xml, L"Manga");
    m_data.AgeRating          = ExtractXMLValue(xml, L"AgeRating");
    m_data.CommunityRating    = ExtractXMLValue(xml, L"CommunityRating");

    // ── Story / characters ──
    m_data.Characters         = ExtractXMLValue(xml, L"Characters");
    m_data.Teams              = ExtractXMLValue(xml, L"Teams");
    m_data.Locations          = ExtractXMLValue(xml, L"Locations");
    m_data.MainCharacterOrTeam = ExtractXMLValue(xml, L"MainCharacterOrTeam");
    m_data.StoryArc           = ExtractXMLValue(xml, L"StoryArc");
    m_data.StoryArcNumber     = ExtractXMLValue(xml, L"StoryArcNumber");
    m_data.SeriesGroup        = ExtractXMLValue(xml, L"SeriesGroup");

    // ── Scan ──
    m_data.ScanInformation    = ExtractXMLValue(xml, L"ScanInformation");

    return true;
}

std::wstring ComicInfoParser::ExtractXMLValue(const std::wstring& xml, const std::wstring& tag)
{
    std::wstring open  = L"<"  + tag + L">";
    std::wstring close = L"</" + tag + L">";

    size_t s = xml.find(open);
    if (s == std::wstring::npos) return L"";
    s += open.length();

    size_t e = xml.find(close, s);
    if (e == std::wstring::npos) return L"";

    return xml.substr(s, e - s);
}

std::wstring ComicInfoParser::EscapeHTML(const std::wstring& input) const
{
    return input;   // Not used for plain-text output; kept for ABI compatibility
}
