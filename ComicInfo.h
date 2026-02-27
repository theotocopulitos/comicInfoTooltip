// ComicInfo.h : ComicInfo.xml parsing and data structures

#pragma once

// ComicInfo.xml data structure
struct ComicInfoData
{
    std::wstring Title;
    std::wstring Series;
    std::wstring Number;
    std::wstring Volume;
    std::wstring Issue;
    std::wstring Publisher;
    std::wstring PublicationDate;
    std::wstring Genre;
    std::wstring Writer;
    std::wstring Penciller;
    std::wstring Inker;
    std::wstring Colorist;
    std::wstring Letterer;
    std::wstring CoverArtist;
    std::wstring Editor;
    std::wstring Summary;
    std::wstring Notes;
    std::wstring Web;
    std::wstring PageCount;
    std::wstring LanguageISO;
    std::wstring Manga;
    std::wstring BlackAndWhite;
    std::wstring AgeRating;
    std::wstring Team;
    std::wstring Location;
    std::wstring Characters;
    std::wstring StoryArc;
    std::wstring StoryArcNumber;
    std::wstring SeriesGroup;
    std::wstring AltSeries;
    std::wstring AltNumber;
    std::wstring AltIssueNumber;
    std::wstring MainCharacterOrTeam;
    std::wstring Review;
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
