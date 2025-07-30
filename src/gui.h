#pragma once

#include <windows.h>
#include <windowsx.h>
#include <gdiplus.h>

#include <unordered_map>
#include <vector>
#include <memory>
#include <cmath>
#include <map>
#include <algorithm>

template<typename T>
struct Initer
{
	T* _copier;
	Initer(T* object) { object->init(); }
	Initer(const Initer& initer) { initer._copier->init(); }
};

template<typename T, typename... Ts>
struct Initee : Ts...
{
	void operator<<=(const auto& l) { l(*(T*)this);  }
	Initee() : Ts{}... {}
	Initee(const Initee& initee) : Ts{initee}...
	{
		T* p = (T*)&initee;
		p->initer._copier = (T*)this;
	}
};

template<typename T, typename... Ts>
struct Delegate
{
	struct Fun
	{
		virtual void operator()(const T& t, const Ts&... ts) = 0;
		virtual ~Fun() {};
	};
	std::map<Fun*, std::shared_ptr<Fun>> _delegates;

	Delegate() = default;
	Delegate(const Delegate&) : Delegate() {};
	Delegate& operator=(const Delegate&) { return *this; };

	template<typename L>
	void operator+=(const L& l)
	{
		struct Funl : Fun, L
		{
			Funl(const L& l) : L(l) {}
			virtual void operator()(const T& t, const Ts&... ts) { L::operator()(t, ts...); };
		} *funl = new Funl{l};
		_delegates[funl] = std::shared_ptr<Fun>{funl};
	};
	void operator()(const T& t, const Ts&... ts)
	{
		for (auto d : _delegates)
		{
			d.second->operator()(t, ts...);
		}
	}
};

template<typename T>
struct Prop
{
	struct Binding
	{
		virtual T operator()() = 0;
		virtual ~Binding() {};
	};

	T val;
	std::shared_ptr<Binding> bind;
	Delegate<const Prop<T>*, T> OnSet;
	Prop(const T& t=T{}) : val{t} {}

	template<typename L, std::enable_if_t<!std::is_convertible<T, L>::value, bool> = true>
	Prop(const L& l) { Bind(l); }

	Prop<T>& operator=(const T& t)
	{
		bind.reset();
		this->val = t;
		OnSet(this, t);
		return *this;
	}

	template<typename L, std::enable_if_t<!std::is_convertible<T, L>::value, bool> = true>
	Prop<T>& operator=(const L& l)
	{
		Bind(l);
		return *this;
	}

	template<typename L>
	void Bind(const L& l)
	{
		struct Bindingl : Binding, L
		{
			Bindingl(const L& l) : L(l) {}
			virtual T operator()() { return L::operator()(); };
		} *bindingl = new Bindingl{l};
		bind.reset(bindingl);
	}
	operator T() const { return bind ? bind->operator()() : val; }
};

struct Node
{
	struct Link
	{
		Node* next;
		Node* prev;
		Link() : next(nullptr), prev(nullptr) {}
		Link(Node* next, Node* prev) : next(next), prev(prev) {}
	};

	Prop<const wchar_t*> Tip;
	const wchar_t* GetTip()
	{
		Node* n = this;
		while (n)
		{
			const wchar_t* tip = n->Tip;
			if (tip) return tip;
			n = n->Parent();
		}
		return nullptr;
	}

	struct
	{
		Prop<float> X, Y, W = NAN, H = NAN;
	} Box;
	bool CollideOutside = false;

	Node* _parent;
	std::unordered_map<Node*, Link> _children;
	std::shared_ptr<Gdiplus::GraphicsPath> Clip;

	Node() : Tip{}, _parent() {}
	Node(const Node& node) : Tip{node.Tip}, Box(node.Box), _parent() {}
	Node& operator=(const Node& node) { Tip = node.Tip; Box = node.Box; return *this; }

	virtual float X() { return Box.X; }
	virtual float Y() { return Box.Y; }
	virtual float W() { return Box.W; }
	virtual float H() { return Box.H; }

	bool Contains(Node* b)
	{
		while (b && b != this) b = b->Parent();
		return this == b;
	}

	float ChildrenWidth()
	{
		float X = std::numeric_limits<float>::min(), x = std::numeric_limits<float>::max();
		for (Node* child = Next(nullptr); child; child = Next(child))
		{
			float _x = child->X();
			x = std::min(x, _x);
			float w = child->W();
			if (w != w) continue;
			X = std::max(X, _x + w);
		}
		return X - x;
	}

	float ChildrenHeight()
	{
		float Y = std::numeric_limits<float>::min(), y = std::numeric_limits<float>::max();
		for (Node* child = Next(nullptr); child; child = Next(child))
		{
			float _y = child->Y();
			y = std::min(y, _y);
			float h = child->H();
			if (h != h) continue;
			Y = std::max(Y, _y + h);
		}
		return Y - y;
	}

	virtual Node* Parent() { return _parent; }
	virtual Node* Next(Node* node) { auto it = _children.find(node); return (it == _children.end()) ? NULL : it->second.next; }
	virtual Node* Prev(Node* node) { auto it = _children.find(node); return (it == _children.end()) ? NULL : it->second.prev; }
	virtual void Add(Node* node)
	{
		if (!_children.count(node))
		{
			node->_parent = this;
			_children[node] = Link{ nullptr, _children[nullptr].prev };
			_children[_children[nullptr].prev].next =  node;
			_children[nullptr].prev = node;
		}
	}

	virtual void Draw(Gdiplus::Graphics* gdi) {}

	virtual void MouseMove(float x, float y, Node* top) {}
	virtual void MouseClick(float x, float y, bool lmb, bool rmb) {}
	virtual void MouseLeftClick(float x, float y, Node* top) {}
	virtual void MouseScroll(Node* top, float v, float h) {}
	virtual void ButtonUp(int vk, int code) {}
	virtual void KeyUp(int vk, int code) {}
	virtual void KeyDown(int vk, int code) {}

	virtual ~Node() {}
};

struct PathGrad : virtual Node
{
	std::vector<Gdiplus::Color> Colors;
	std::vector<float> Widths;
	std::shared_ptr<Gdiplus::GraphicsPath> Path;
	struct
	{
		float X, Y;
	} Center = {0.5, 0.5};

	virtual float W()
	{
		Gdiplus::RectF bb;
		Path ? Path->GetBounds(&bb) : 0;
		return bb.Width;
	}

	virtual float H()
	{
		Gdiplus::RectF bb;
		Path ? Path->GetBounds(&bb) : 0;
		return bb.Height;
	}

	virtual void Draw(Gdiplus::Graphics* gdi)
	{
		Gdiplus::PathGradientBrush brush(Path.get());

		float w = W()/2, h = H()/2;
		float sx = (w > h) ? 1 - h/w : 0.0;
		float sy = (w > h) ? 0.0 : 1 - w/h;
		brush.SetFocusScales(sx, sy);
		brush.SetGammaCorrection(true);
		brush.SetCenterPoint(Gdiplus::PointF(2*w*Center.X, 2*h*Center.Y));

		float dist = std::min(w, h);
		std::vector<float> positions = {0.0};
		float d = 0;
		for (auto w : Widths)
		{
			d += w;
			w = d/dist;
			positions.push_back(w);
		}
		positions.push_back(1.0);
		auto colors = Colors;
		colors.push_back(Colors.back());
		brush.SetInterpolationColors(colors.data(), positions.data(), positions.size());
		gdi->FillPath(&brush, Path.get());
	}

};

struct Ellipze : PathGrad
{
	virtual float W() { return Node::W(); }
	virtual float H() { return Node::H(); }
	virtual void Draw(Gdiplus::Graphics* gdi)
	{
		using namespace Gdiplus;
		std::shared_ptr<GraphicsPath> rrect = std::make_shared<GraphicsPath>();
		rrect->AddEllipse(0.0, 0.0, W(), H());
		Path = rrect;
		PathGrad::Draw(gdi);
	}
};

struct RectGrad : Initee<RectGrad, PathGrad>
{
	float R;
	virtual float W() { return Node::W(); }
	virtual float H() { return Node::H(); }
	virtual void Draw(Gdiplus::Graphics* gdi)
	{
		using namespace Gdiplus;
		float w = 2*R;
		float width = W(), height = H();
		if (width <= 0|| height <= 0 || width != width || height != height) return;
		std::shared_ptr<GraphicsPath> rrect = std::make_shared<GraphicsPath>();
		rrect->AddArc(0.0, 0.0, w, w, 180.0, 90.0);
		rrect->AddArc(width - w, 0.0, w, w, 270.0, 90.0);
		rrect->AddArc(width - w, height - w, w, w, 0.0, 90.0);
		rrect->AddArc(0.0, height - w, w, w, 90.0, 90.0);
		Path = rrect;

		if (width < 50 && height < 50)
		{
			PathGrad::Draw(gdi);
			return;
		}

		float r = 0;
		for (auto w : Widths) { r += w; }
		r = 2*std::max(r, R);
		Rect clip(r, r, width - 2*r, height - 2*r);
		GraphicsState state = gdi->Save();
		gdi->SetClip(clip, CombineModeExclude);
		PathGrad::Draw(gdi);
		gdi->Restore(state);
		SolidBrush red(Colors.back());
		clip.X--;
		clip.Y--;
		clip.Width++;
		clip.Height++;
		gdi->FillRectangle(&red, clip);
	}

	Initer<RectGrad> initer = this;
	void init() {}
};

struct ScrollPane : virtual Node, Initee<ScrollPane>
{
	Node content;
	struct : RectGrad
	{
		float oy = NAN;
		virtual void MouseMove(float x, float y, Node* top)
		{
			oy = (top == this) ? oy : NAN;
			Box.W = (top == this) ? 15 : 5;
			if (oy == oy) Box.Y = std::min(std::max(0.f, Box.Y + y - oy), Parent()->H() - H());
		}
		virtual void MouseLeftClick(float x, float y, Node* top) { oy = (top == this) ? y : NAN; }
	} vscroll;
	struct : RectGrad
	{
		float ox = NAN;
		virtual void MouseMove(float x, float y, Node* top)
		{
			ox = (top == this) ? ox : NAN;
			Box.H = (top == this) ? 15 : 5;
			if (ox == ox) Box.X = std::min(std::max(0.f, Box.X + x - ox), Parent()->W() - W());
		}
		virtual void MouseLeftClick(float x, float y, Node* top) { ox = (top == this) ? x : NAN; }
	} hscroll;

	virtual void MouseScroll(Node* top, float v, float h)
	{
		if (!this->Contains(top)) return;
		vscroll.Box.Y = vscroll.Box.Y - 10*v;
		hscroll.Box.X = hscroll.Box.X + 10*h;
	}

	virtual void Add(Node* node)
	{
		content.Add(node);
	}

	Initer<ScrollPane> initer = this;
	void init()
	{
		Node::Add(&content);
		Node::Add(&vscroll);
		Node::Add(&hscroll);
		auto vh = [this]()
		{
			float H = content.ChildrenHeight();
			float h = this->H();
			if (h >= H) return 0.f;
			return h*h/H;
		};
		auto hw = [this]()
		{
			float W = content.ChildrenWidth();
			float w = this->W();
			if (w >= W) return 0.f;
			return w*w/W;
		};
		vscroll.Box = { .X = [this](){ return W() - vscroll.W(); }, .Y = 0, .W = 5, .H = vh };
		hscroll.Box = { .X = 0, .Y = [this](){ return H() - hscroll.H(); }, .W = hw, .H = 5, };

		auto x = [this]()
		{
			float sx = W() - hscroll.W();
			float r = content.ChildrenWidth()/W();
			if (r < 1.0 || hscroll.Box.X < 0) hscroll.Box.X = 0;
			else if (hscroll.Box.X > sx) hscroll.Box.X = sx;
			return -hscroll.X()*r;
		};
		auto y = [this]()
		{
			float sy = H() - vscroll.H();
			float r = content.ChildrenHeight()/H();
			if (r < 1.0 || vscroll.Box.Y < 0) vscroll.Box.Y = 0;
			else if (vscroll.Box.Y > sy) vscroll.Box.Y = sy;
			return -vscroll.Y()*r;
		};
		content.Box = { .X = x, .Y = y };
	}
};

struct TextBox : virtual Node, Initee<TextBox>
{
	Prop<const WCHAR*> Txt = L"";
	Prop<Gdiplus::Color> Color = Gdiplus::Color(255, 0, 0, 0);
	Prop<Gdiplus::REAL> Size = 12;
	Prop<Gdiplus::FontFamily*> Fam;
	Gdiplus::StringFormat* Format = Gdiplus::StringFormat::GenericDefault()->Clone();
	Gdiplus::RectF _bounds = {};

	virtual void CalcBounds()
	{
		if (Txt && Fam)
		{
			Gdiplus::GraphicsPath path;
			path.AddString(Txt, -1, Fam, Gdiplus::FontStyleRegular, Size, Gdiplus::PointF(0, 0), Format);
			Gdiplus::Pen pen(Gdiplus::Color(255, 0, 0, 0));
			Gdiplus::Status s = path.GetBounds(&_bounds, NULL, &pen);
		}
		else
		{
			_bounds = {};
		}
	}
	Initer<TextBox> initer = this;
	void init()
	{
		Txt.OnSet += [this](auto, auto& val){ CalcBounds(); };
		Fam.OnSet += [this](auto, auto& val){ CalcBounds(); };
		Size.OnSet += [this](auto, auto& val){ CalcBounds(); };
		CalcBounds();
	}

	virtual float W()
	{
		float w = Node::W();
		return (w == w) ? w : _bounds.Width;
	}

	virtual float H()
	{
		float h = Node::H();
		return (h == h) ? h : _bounds.Height;
	}

	virtual void Draw(Gdiplus::Graphics* gdi)
	{
		using namespace Gdiplus;
		if (Txt && Fam)
		{
			Gdiplus::GraphicsPath path;
			Gdiplus::SolidBrush solidBrush(Color);
			RectF box = {0, 0, W(), H()};
			path.AddString(Txt, -1, Fam, Gdiplus::FontStyleRegular, Size, box, Format);
			Pen pen(Gdiplus::Color(240, 0, 0, 0), 2);
			gdi->DrawPath(&pen, &path);
			gdi->FillPath(&solidBrush, &path);
		}
	}
};

inline static void Draw(Gdiplus::Graphics* gdi, Node* root, float w, float h)
{
	Gdiplus::GraphicsState state = gdi->Save();
	gdi->TranslateTransform(root->X(), root->Y());
	root->Draw(gdi);
	if (root->Clip)
	{
		gdi->SetClip(root->Clip.get(), Gdiplus::CombineModeIntersect);
	}
	for (Node* child = root->Next(0); child; child = root->Next(child))
	{
		Draw(gdi, child, w, h);
	}
	gdi->Restore(state);
}

inline Node* gFocus;

inline static Node* Top(Node* node, float x, float y)
{
	float w =  node->W(), h =  node->H();
	bool inside = w != w || h != h || (x >= 0 && y >= 0 && x < w && y < h);
	if (!inside)
	if (!node->CollideOutside)
		return nullptr;
	for (Node* child = node->Prev(0); child; child = node->Prev(child))
	{
		Node* hit = Top(child, x - child->X(), y - child->Y());
		if (hit) return hit;
	}
	return (inside && w == w && h == h) ? node : nullptr;
}

inline static void MouseMove(Node* root, float x, float y, Node* top)
{
	root->MouseMove(x, y, top);
	for (Node* child = root->Next(0); child; child = root->Next(child))
	{
		MouseMove(child, x - child->X(), y - child->Y(), top);
	}
}

inline static Node* MouseMove(Node* root, float x, float y)
{
	Node* top = Top(root, x, y);
	MouseMove(root, x, y, gFocus ? gFocus : top);
	return gFocus ? nullptr: top;
}

inline static Node* MouseClick(Node* root, float x, float y)
{
	float w =  root->W(), h =  root->H();
	bool inside = w == w && h == h && x >= 0 && y >= 0 && x < w && y < h;
	Node* top = inside ? root : NULL;
	for (Node* child = root->Next(0); child; child = root->Next(child))
	{
		Node* hit = MouseClick(child, x - child->X(), y - child->Y());
		top = hit ? hit : top;
	}
	root->MouseLeftClick(x, y, top);
	gFocus = top;
	return top;
}

inline static void ButtonUp(Node* root, int vk, int code)
{
	for (Node* child = root->Next(0); child; child = root->Next(child))
	{
		ButtonUp(child, vk, code);
	}
	root->ButtonUp(vk, code);
}

static inline Node* MouseScroll(Node* root, float x, float y, float v, float h, Node* top)
{
	root->MouseScroll(top, v, h);
	for (Node* child = root->Next(0); child; child = root->Next(child))
	{
		MouseScroll(child, x - child->X(), y - child->Y(), v, h, top);
	}
	return nullptr;
}

static inline Node* MouseScroll(Node* root, float x, float y, float v, float h)
{
	MouseScroll(root, x, y, v, h, Top(root, x, y));
	return nullptr;
}

