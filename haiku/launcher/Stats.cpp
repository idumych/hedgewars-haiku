// The statistics window: frontend-qt6's PageGameStats as a native window.
// The medal and award pictures are the Qt frontend's, in our resources.
#include "Stats.h"
#include "StatsData.h"

#include <Bitmap.h>
#include <Button.h>
#include <GroupView.h>
#include <LayoutBuilder.h>
#include <StringView.h>
#include <TranslationUtils.h>
#include <Window.h>

#include <algorithm>

// a picture from our resources, at its own size
class Picture : public BView {
public:
	Picture(const char* name) : BView("picture", B_WILL_DRAW)
	{
		fBitmap = BTranslationUtils::GetBitmap('PNG ', name);
		BSize size = fBitmap ? BSize(fBitmap->Bounds().Width(), fBitmap->Bounds().Height()) : BSize(0, 0);
		SetExplicitSize(size);
		SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	}
	~Picture() { delete fBitmap; }
	void Draw(BRect) override
	{
		if (fBitmap) {
			SetDrawingMode(B_OP_ALPHA);
			DrawBitmap(fBitmap, B_ORIGIN);
		}
	}
private:
	BBitmap* fBitmap;
};

static rgb_color to_rgb(uint32_t c) { return make_color((c >> 16) & 255, (c >> 8) & 255, c & 255); }

// one line per clan, a point per turn; zero is always shown when every value is positive
class Graph : public BView {
public:
	Graph(const std::map<uint32_t, std::vector<int>>& health)
		: BView("graph", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE), fHealth(health)
	{
		SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);
		SetExplicitMinSize(BSize(360, 160));
	}
	void Draw(BRect) override
	{
		size_t points = 0;
		int lo = 0, hi = 1;
		for (const auto& c : fHealth)
			for (int v : c.second) {
				points = std::max(points, c.second.size());
				lo = std::min(lo, v);
				hi = std::max(hi, v);
			}
		BRect r = Bounds().InsetByCopy(6, 6);
		SetHighUIColor(B_CONTROL_BORDER_COLOR);
		StrokeRect(Bounds());
		SetPenSize(2);
		for (const auto& c : fHealth) {
			SetHighColor(to_rgb(c.first));
			for (size_t t = 1; t < c.second.size(); t++) {
				auto at = [&](size_t i) {
					return BPoint(r.left + r.Width() * i / (points - 1),
						r.bottom - r.Height() * (c.second[i] - lo) / (hi - lo));
				};
				StrokeLine(at(t - 1), at(t));
			}
		}
		SetPenSize(1);
	}
private:
	std::map<uint32_t, std::vector<int>> fHealth;
};

static BStringView* heading(const char* text, float scale)
{
	BStringView* s = new BStringView("heading", text);
	BFont f(be_bold_font);
	f.SetSize(f.Size() * scale);
	s->SetFont(&f);
	return s;
}

void show_stats(const std::vector<std::string>& lines, BRect over)
{
	StatsData d = parse_stats(lines);
	if (d.winner.empty() && d.ranks.empty() && d.awards.empty())
		return;
	BWindow* w = new BWindow(BRect(0, 0, 100, 100), "Game statistics", B_TITLED_WINDOW,
		B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS | B_CLOSE_ON_ESCAPE);
	BButton* close = new BButton("Close", new BMessage(B_QUIT_REQUESTED));
	BLayoutBuilder::Group<> b(w, B_VERTICAL);
	b.SetInsets(B_USE_WINDOW_SPACING);
	if (!d.winner.empty())
		b.Add(heading(d.winner.c_str(), 1.4));

	if (!d.ranks.empty()) {
		BGridLayout* grid;
		b.Add(heading("Ranking", 1.0));
		b.AddGrid(B_USE_SMALL_SPACING, B_USE_SMALL_SPACING).GetLayout(&grid).End();
		for (size_t i = 0; i < d.ranks.size(); i++) {
			const StatsData::Rank& r = d.ranks[i];
			std::string medal = "StatsMedal" + std::to_string(std::min(std::max(r.position, 1), 4));
			BStringView* team = new BStringView("team", (std::to_string(r.position) + ". " + r.team).c_str());
			team->SetHighColor(to_rgb(r.color));
			grid->AddView(new Picture(medal.c_str()), 0, i);
			grid->AddView(team, 1, i);
			grid->AddView(new BStringView("points", r.points.c_str()), 2, i);
		}
		grid->AddItem(BSpaceLayoutItem::CreateGlue(), 3, 0);
	}

	if (!d.awards.empty()) {
		BGridLayout* grid;
		b.AddGrid(B_USE_SMALL_SPACING, B_USE_SMALL_SPACING).GetLayout(&grid).End();
		for (size_t i = 0; i < d.awards.size(); i++) {
			grid->AddView(new Picture(d.awards[i].icon.c_str()), 0, i);
			grid->AddView(new BStringView("award", d.awards[i].text.c_str()), 1, i);
		}
	}

	size_t points = 0;
	for (const auto& c : d.health)
		points = std::max(points, c.second.size());
	if (points >= 2)	// a single point isn't a graph
		b.Add(heading(d.graphTitle.c_str(), 1.0)).Add(new Graph(d.health));

	b.AddGroup(B_HORIZONTAL).AddGlue().Add(close).End();
	w->SetDefaultButton(close);
	w->CenterIn(over);
	w->Show();
}
