// ComicInfo.h : ComicInfo.xml parsing and data structures

#pragma once

// ComicInfo.xml data structure  (v2.1 schema)
struct ComicInfoData
{
    // ── Identity ──
    std::wstring Title;
    std::wstring Series;
    std::wstring Number;
    std::wstring Count;              // total issues in series
    std::wstring Volume;

    // ── Alternate series ──
    std::wstring AlternateSeries;
    std::wstring AlternateNumber;
    std::wstring AlternateCount;

    // ── Text blocks ──
    std::wstring Summary;
    std::wstring Notes;
    std::wstring Review;

    // ── Date ──
    std::wstring Year;
    std::wstring Month;
    std::wstring Day;

    // ── Creative team ──
    std::wstring Writer;
    std::wstring Penciller;
    std::wstring Inker;
    std::wstring Colorist;
    std::wstring Letterer;
    std::wstring CoverArtist;
    std::wstring Editor;
    std::wstring Translator;         // v2.1

    // ── Publishing ──
    std::wstring Publisher;
    std::wstring Imprint;            // v2.1
    std::wstring Genre;
    std::wstring Tags;               // v2.0
    std::wstring Web;
    std::wstring Format;
    std::wstring GTIN;               // v2.1 (ISBN / barcode)

    // ── Numeric / enum ──
    std::wstring PageCount;
    std::wstring LanguageISO;
    std::wstring BlackAndWhite;      // Yes / No / Unknown
    std::wstring Manga;              // Yes / No / YesAndRightToLeft / Unknown
    std::wstring AgeRating;
    std::wstring CommunityRating;    // 0.0–5.0

    // ── Story / characters ──
    std::wstring Characters;
    std::wstring Teams;
    std::wstring Locations;
    std::wstring MainCharacterOrTeam;
    std::wstring StoryArc;
    std::wstring StoryArcNumber;
    std::wstring SeriesGroup;

    // ── Scan ──
    std::wstring ScanInformation;
};

class ComicInfoParser
{
public:
    ComicInfoParser();
    ~ComicInfoParser();

    // Parse ComicInfo.xml from string content
    bool ParseFromXML(const std::wstring& xmlContent);
    
    // Get parsed data
    const ComicInfoData& GetData() const { return m_data; }
    
    // Format data for display (plain text)
    std::wstring FormatAsText() const;
    
    // Clear all data
    void Clear();

private:
    ComicInfoData m_data;
    
    // Helper methods for XML parsing
    bool ParseXMLElement(const std::wstring& xmlContent);
    std::wstring ExtractXMLValue(const std::wstring& xmlContent, const std::wstring& tagName);
    std::wstring EscapeHTML(const std::wstring& input) const;
};
