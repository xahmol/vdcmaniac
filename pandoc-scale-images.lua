-- Fix the Screenshots section's GFM tables for PDF output: pandoc's LaTeX
-- writer renders plain pipe tables as non-wrapping l/c/r columns, which
-- overflow the page when cells hold images and long caption text. Forcing
-- explicit, equal column widths (colspecs, summing to just under the full
-- text width -- 0.94, not 1.0, to leave room for longtable's own
-- \tabcolsep padding on every column) switches the writer to wrapping
-- p{width} columns instead. README.md itself stays plain GFM (no width
-- attributes, no table markup changes) so GitHub's own rendering is
-- unaffected -- both fixes below apply only in this PDF conversion pass.
--
-- Image width inside a table cell is set relative to the FULL page
-- linewidth, not the column's own width: p{width} table columns do not
-- rescope \linewidth to the column (confirmed empirically -- a width set
-- as a plain percentage rendered identically regardless of which column
-- the image was in, overflowing every column past the first). Each
-- image's width is therefore computed here as a fraction of the table's
-- own column count, in two sequential passes so the Table pass's own
-- per-image sizing always wins over the fallback below.
local function fix_table(tbl)
  local n = #tbl.colspecs
  for _, spec in ipairs(tbl.colspecs) do
    spec[2] = 0.94 / n
  end
  local img_width = string.format("%.4f\\linewidth", 0.90 / n)
  return tbl:walk({
    Image = function(img)
      img.attributes.width = img_width
      return img
    end
  })
end

-- Standalone images outside any table (the end-credits screenshot) --
-- only touches images the Table pass above left unsized.
local function fix_standalone_image(img)
  if img.attributes.width == nil or img.attributes.width == "" then
    img.attributes.width = "60%"
  end
  return img
end

return {
  { Table = fix_table },
  { Image = fix_standalone_image },
}
