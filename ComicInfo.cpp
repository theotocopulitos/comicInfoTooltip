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

std::wstring ComicInfoParser::FormatAsText() const
{
    std::wstring t;

    // ── Title block ─────────────────────────────────────────────────────────
    std::wstring titleLine;
    if (!m_data.Series.empty())
    {
        titleLine = m_data.Series;
        if (!m_data.Volume.empty()) { titleLine += L" \u2022 Vol. "; titleLine += m_data.Volume; }
        if (!m_data.Number.empty()) { titleLine += L" #"; titleLine += m_data.Number; }
    }
    else if (!m_data.Title.empty())
    {
        titleLine = m_data.Title;
    }

    if (!titleLine.empty())
    {
        t += titleLine;
        t += L"\r\n";
    }

    if (!m_data.AltSeries.empty())
    {
        t += L"  (";
        t += m_data.AltSeries;
        if (!m_data.AltNumber.empty()) { t += L" #"; t += m_data.AltNumber; }
        t += L")\r\n";
    }

    // ── Publication ──────────────────────────────────────────────────────────
    bool hasPub = !m_data.Publisher.empty() || !m_data.PublicationDate.empty()
               || !m_data.Genre.empty()     || !m_data.LanguageISO.empty()
               || !m_data.AgeRating.empty() || !m_data.PageCount.empty()
               || !m_data.BlackAndWhite.empty() || !m_data.Manga.empty();
    if (hasPub)
    {
        t += kSep; t += L"\r\n";

        // Publisher + date on one line if both present
        if (!m_data.Publisher.empty() && !m_data.PublicationDate.empty())
        {
            t += L"\u2022 "; t += m_data.Publisher;
            t += L"  ("; t += m_data.PublicationDate; t += L")\r\n";
        }
        else
        {
            Line(t, L"\u2022 Publisher:  ", m_data.Publisher);
            Line(t, L"\u2022 Published:  ", m_data.PublicationDate);
        }

        Line(t, L"\u2022 Genre:      ", m_data.Genre);
        Line(t, L"\u2022 Language:   ", m_data.LanguageISO);
        Line(t, L"\u2022 Age Rating: ", m_data.AgeRating);

        if (!m_data.PageCount.empty())
        {
            t += L"\u2022 Pages:      "; t += m_data.PageCount; t += L"\r\n";
        }

        // Flags: B&W, Manga
        std::wstring flags;
        if (m_data.BlackAndWhite == L"Yes") flags += L"Black & White  ";
        if (m_data.Manga         == L"Yes" || m_data.Manga == L"YesAndRightToLeft")
            flags += L"Manga";
        if (!flags.empty()) { t += L"\u2022 Flags:      "; t += flags; t += L"\r\n"; }
    }

    // ── Creative team ────────────────────────────────────────────────────────
    bool hasCrew = !m_data.Writer.empty()   || !m_data.Penciller.empty()
                || !m_data.Inker.empty()     || !m_data.Colorist.empty()
                || !m_data.Letterer.empty()  || !m_data.CoverArtist.empty()
                || !m_data.Editor.empty();
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
    }

    // ── Story / characters ───────────────────────────────────────────────────
    bool hasStory = !m_data.StoryArc.empty()  || !m_data.Characters.empty()
                 || !m_data.Team.empty()       || !m_data.Location.empty()
                 || !m_data.SeriesGroup.empty();
    if (hasStory)
    {
        t += kSep; t += L"\r\n";
        if (!m_data.StoryArc.empty())
        {
            std::wstring arc = m_data.StoryArc;
            if (!m_data.StoryArcNumber.empty()) { arc += L" (Part "; arc += m_data.StoryArcNumber; arc += L")"; }
            Line(t, L"\u2605 Story Arc:   ", arc);
        }
        Line(t, L"\u2605 Characters:  ", m_data.Characters);
        Line(t, L"\u2605 Team:         ", m_data.Team);
        Line(t, L"\u2605 Location:     ", m_data.Location);
        Line(t, L"\u2605 Series Group: ", m_data.SeriesGroup);
    }

    // ── Summary ──────────────────────────────────────────────────────────────
    if (!m_data.Summary.empty())
    {
        t += kSep; t += L"\r\n";
        WrapAppend(t, m_data.Summary, 72);
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
    m_data.Title            = ExtractXMLValue(xml, L"Title");
    m_data.Series           = ExtractXMLValue(xml, L"Series");
    m_data.Number           = ExtractXMLValue(xml, L"Number");
    m_data.Volume           = ExtractXMLValue(xml, L"Volume");
    m_data.Issue            = ExtractXMLValue(xml, L"Issue");
    m_data.Publisher        = ExtractXMLValue(xml, L"Publisher");
    m_data.PublicationDate  = ExtractXMLValue(xml, L"PublicationDate");
    m_data.Genre            = ExtractXMLValue(xml, L"Genre");
    m_data.Writer           = ExtractXMLValue(xml, L"Writer");
    m_data.Penciller        = ExtractXMLValue(xml, L"Penciller");
    m_data.Inker            = ExtractXMLValue(xml, L"Inker");
    m_data.Colorist         = ExtractXMLValue(xml, L"Colorist");
    m_data.Letterer         = ExtractXMLValue(xml, L"Letterer");
    m_data.CoverArtist      = ExtractXMLValue(xml, L"CoverArtist");
    m_data.Editor           = ExtractXMLValue(xml, L"Editor");
    m_data.Summary          = ExtractXMLValue(xml, L"Summary");
    m_data.Notes            = ExtractXMLValue(xml, L"Notes");
    m_data.Web              = ExtractXMLValue(xml, L"Web");
    m_data.PageCount        = ExtractXMLValue(xml, L"PageCount");
    m_data.LanguageISO      = ExtractXMLValue(xml, L"LanguageISO");
    m_data.Manga            = ExtractXMLValue(xml, L"Manga");
    m_data.BlackAndWhite    = ExtractXMLValue(xml, L"BlackAndWhite");
    m_data.AgeRating        = ExtractXMLValue(xml, L"AgeRating");
    m_data.Team             = ExtractXMLValue(xml, L"Team");
    m_data.Location         = ExtractXMLValue(xml, L"Location");
    m_data.Characters       = ExtractXMLValue(xml, L"Characters");
    m_data.StoryArc         = ExtractXMLValue(xml, L"StoryArc");
    m_data.StoryArcNumber   = ExtractXMLValue(xml, L"StoryArcNumber");
    m_data.SeriesGroup      = ExtractXMLValue(xml, L"SeriesGroup");
    m_data.AltSeries        = ExtractXMLValue(xml, L"AltSeries");
    m_data.AltNumber        = ExtractXMLValue(xml, L"AltNumber");
    m_data.AltIssueNumber   = ExtractXMLValue(xml, L"AltIssueNumber");
    m_data.MainCharacterOrTeam = ExtractXMLValue(xml, L"MainCharacterOrTeam");
    m_data.Review           = ExtractXMLValue(xml, L"Review");
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
