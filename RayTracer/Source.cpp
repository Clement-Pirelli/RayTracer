#include <cmath>

//CONSTANTS
const double RENDER_DISTANCE = 9999.0;

static inline double mix(double a, double b, double t)
{
	return a * (1.0 - t) + b * t;
}

static inline double min(double a, double b)
{
	return mix(a, b, (double)(a > b));
}

static inline double max(double a, double b)
{
	return mix(a, b, (double)(a < b));
}

static inline double clamp(double giventerm, double givenmin, double givenmax)
{ 
	giventerm = min(giventerm, givenmax);
	giventerm = max(giventerm, givenmin);

	return giventerm;
}

union vec3
{
	struct
	{
		double r, g, b;
	};

	struct
	{
		double x, y, z;
	};

	vec3(){ x = 0; y = 0; z = 0; }
	vec3(double givenX, double givenY, double givenZ) : x(givenX), y(givenY), z(givenZ){}
	vec3(double givenVal){ x = givenVal; y = givenVal; z = givenVal; }

	vec3 operator -(vec3 b){ return vec3(x - b.x, y - b.y, z - b.z); }
	vec3 operator +(vec3 b){ return vec3(x + b.x, y + b.y, z + b.z); }
	vec3 operator /(double b){ return vec3(x / b, y / b, z / b); }
	vec3 operator *(double b){ return vec3(x * b, y * b, z * b); }
	void operator /=(double b){ x /= b; y /= b; z /= b; }
	void operator *=(double b){ x *= b; y *= b; z *= b; }
	void operator +=(vec3 b){ x += b.x; y += b.y; z += b.z; }

	static inline double dot(vec3 a, vec3 b){ return a.x*b.x + a.y*b.y + a.z*b.z; }
	static inline double magnitude(vec3 a){ return sqrt(a.x*a.x + a.y*a.y + a.z*a.z); }

	static inline vec3 normalize(vec3 a){ return a / magnitude(a); }
};

static inline vec3 clamp(vec3 giventerm, double givenmin, double givenmax)
{
	return vec3(clamp(giventerm.x, givenmin, givenmax), clamp(giventerm.y, givenmin, givenmax), clamp(giventerm.z, givenmin, givenmax));
}

static inline vec3 mix(vec3 a, vec3 b, double t)
{
	return vec3
	(
		mix(a.x, b.x, t),
		mix(a.y, b.y, t),
		mix(a.z, b.z, t)
	);
}

//ray data, used to get the pixel color by sending (or casting) a ray per pixel into the scene
struct ray
{
	vec3 origin;
	vec3 direction;

	ray(vec3 givenOrigin, vec3 givenDirection) : origin(givenOrigin), direction(givenDirection){}
};

struct material
{
	double diff;
	double spec;
	double gloss;

	material(double givenParams) { diff = spec = gloss = givenParams; }
	material(double givenDiff, double givenSpec, double givenGloss) : diff(givenDiff), spec(givenSpec), gloss(givenGloss) {}
};

static inline material mix(material a, material b, double t)
{
	return material(mix(a.diff, b.diff, t), mix(a.spec, b.spec, t), mix(a.gloss, b.gloss, t));
}

//one-color sphere
struct sphere
{
	vec3 origin;
	vec3 color;
	double radius;
	material mat;
	
	sphere(vec3 givenOrigin, vec3 givenColor, double givenRadius, material givenMaterial) : origin(givenOrigin), color(givenColor), radius(givenRadius), mat(givenMaterial){}
};

//point light
struct pointLight
{
	vec3 origin;
	double intensity;

	pointLight(vec3 givenOrigin, double givenIntensity) : origin(givenOrigin), intensity(givenIntensity) {}
};

struct directionalLight
{
	vec3 direction;
	double intensity;

	directionalLight(vec3 givenDir, double givenIntensity) : direction(givenDir), intensity(givenIntensity){}
};

struct intersection
{
	static const intersection MISS;

	//distance from the origin of the ray to the intersection
	double dist;
	//color of the primitive at the intersection
	vec3 color;
	//normal of the primitive at the intersection
	vec3 normal;

	material mat;

	intersection(double givenDist, vec3 givenCol, vec3 givenNormal, material givenMat) : dist(givenDist), color(givenCol), normal(givenNormal), mat(givenMat){}

};

const intersection intersection::MISS = intersection(RENDER_DISTANCE, vec3(22.0), vec3(.0), material(.0));

static inline intersection mix(intersection a, intersection b, double t)
{
	return intersection
	(
		mix(a.dist, b.dist, t),
		mix(a.color, b.color, t),
		mix(a.normal, b.normal, t),
		mix(a.mat, b.mat, t)
	);
}

static inline double rayCast(ray &pRay, sphere &pSphere)
{
	double radius2 = pSphere.radius*pSphere.radius;
	vec3 L = pSphere.origin - pRay.origin;
	double tca = vec3::dot(L, pRay.direction);
	double d2 = vec3::dot(L, L) - tca * tca;
	if (d2 > radius2) return false;
	double thc = sqrt(radius2 - d2);
	double t0 = tca - thc;
	double t1 = tca + thc;
	double t = min(t0, t1);
	return t;
}

static inline intersection intersect(ray &pRay, sphere &pSphere)
{
	double t = rayCast(pRay, pSphere);
	intersection hit = intersection(t, pSphere.color, (pSphere.origin - (pRay.origin + pRay.direction*t)) / pSphere.radius, pSphere.mat);
	return mix(intersection::MISS, hit, (double)(t < .0));
}


static inline bool isInShadow(ray &pRay, sphere &pSphere, vec3 &lightPos, double minDistance)
{
	double t = rayCast(pRay, pSphere);
	return (t > minDistance) && (t < vec3::magnitude(pRay.origin - lightPos));
}

//base code provided Tommi Lipponen
#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>
#include <stdio.h>

struct RenderTarget {
	HDC device_;
	int width_;
	int height_;
	unsigned* data_;
	BITMAPINFO info_;

	RenderTarget(HDC device, int width, int height)
		: device_(device), width_(width), height_(height), data_(nullptr)
	{
		data_ = new unsigned[width*height];
		info_.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		info_.bmiHeader.biWidth = width_;
		info_.bmiHeader.biHeight = -height_;
		info_.bmiHeader.biPlanes = 1;
		info_.bmiHeader.biBitCount = 32;
		info_.bmiHeader.biCompression = BI_RGB;
	}
	~RenderTarget() { delete[] data_; }
	inline int  size() const {
		return width_ * height_;
	}
	void clear(unsigned color) {
		const int count = size();
		for (int i = 0; i < count; i++) {
			data_[i] = color;
		}
	}
	inline void pixel(int x, int y, unsigned color) {
		if (y < 0 || y >= width_) { return; }
		if (x < 0 || x >= height_) { return; }
		data_[y*width_ + x] = color;
	}
	void present() {
		StretchDIBits(device_,
			0, 0, width_, height_,
			0, 0, width_, height_,
			data_, &info_,
			DIB_RGB_COLORS, SRCCOPY);
	}
};

static unsigned
make_color(unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha)
{
	unsigned result = 0;
	if (alpha > 0)
	{
		result |= ((unsigned)red << 16);
		result |= ((unsigned)green << 8);
		result |= ((unsigned)blue << 0);
		result |= ((unsigned)alpha << 24);
	}
	return result;
}

static unsigned long long
get_ticks64()
{
	static LARGE_INTEGER start = {};
	static unsigned long long factor = 0;
	if (!factor)
	{
		LARGE_INTEGER frequency = {};
		QueryPerformanceFrequency(&frequency);
		factor = frequency.QuadPart / 1000000;
		QueryPerformanceCounter(&start);
	}
	LARGE_INTEGER now = {};
	QueryPerformanceCounter(&now);
	auto diff = now.QuadPart - start.QuadPart;
	return (diff / factor);
}

static LRESULT CALLBACK
Win32DefaultProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
	switch (message) {
	case WM_CLOSE: { PostQuitMessage(0); } break;
	default: { return DefWindowProcA(window, message, wparam, lparam); } break;
	}
	return 0;
}

const double ambientLight = .0;
const unsigned int sphereCount = 2;
const unsigned int lightCount = 2;
const unsigned int maxBounces = 100;


static inline vec3 calcLighting(ray pRay, pointLight *pLights, sphere *pSpheres)
{
	vec3 finalColor = vec3(.0);
	intersection finalIntersection = intersection::MISS;
	for (unsigned int lightIndex = 0; lightIndex < lightCount; lightIndex++)
	{
		//for every sphere
		for (unsigned int sphereIndex = 0; sphereIndex < sphereCount; sphereIndex++)
		{
			intersection currentIntersection = intersect(pRay, pSpheres[sphereIndex]);
			finalIntersection = mix(finalIntersection, currentIntersection, (double)(currentIntersection.dist < finalIntersection.dist));
		}
		vec3 intersectionPoint = pRay.origin + pRay.direction * finalIntersection.dist;
		vec3 lightDir = pLights[lightIndex].origin - intersectionPoint;
		double lightDistance = vec3::magnitude(lightDir);
		double lightIntensity = pLights[lightIndex].intensity / (lightDistance*lightDistance);
		double diffuseTerm = clamp(vec3::dot(lightDir / lightDistance, finalIntersection.normal), .0, 1.0) * finalIntersection.mat.diff;


		vec3 reflection = finalIntersection.normal * 2.0 * vec3::dot(finalIntersection.normal, lightDir) - lightDir;
		//specular term
		//Thanks to UglySwedishFish#3207 on discord for their help with this :
		//dot products range from -1 to 1, so the dot product of the reflection and the ray's direction
		//has to be clamped so that a negative value doesn't get squared into a positive value
		const double specularTerm = pow(clamp(vec3::dot(vec3::normalize(reflection), pRay.direction), .0, 1.0), finalIntersection.mat.gloss) * finalIntersection.mat.spec;

		bool inShadow = false;
		for (unsigned int sphereIndex = 0; sphereIndex < sphereCount; sphereIndex++)
		{
			ray shadowRay = ray(intersectionPoint, vec3::normalize(pLights[lightIndex].origin - intersectionPoint));
			inShadow = inShadow || isInShadow(shadowRay, pSpheres[sphereIndex], pLights[lightIndex].origin, .00001);
		}

		finalColor += finalIntersection.color * lightIntensity * ((diffuseTerm + specularTerm)*(double)(!inShadow) + ambientLight);
	}

	finalColor = clamp(finalColor, .0, 255.0);
	//add background color
	finalColor = mix(finalColor, intersection::MISS.color, (finalIntersection.dist == intersection::MISS.dist));

	return finalColor;
}


int CALLBACK
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCode) {
	const char* window_title = "MinimalWindowPixelOnScreen";

	sphere spheres[sphereCount]
	{
		sphere(vec3(.0, .0, 4.0), vec3(.0, .0, 255.0), .6, material(.9, .1, 64.0)),
		sphere(vec3(.0, .0, -1.0), vec3(255.0, .0, 255.0), .1, material(.5, .9, 8.0))
	};

	pointLight lights[lightCount]
	{
		pointLight(vec3(.0, .0, -6.0), 100.0),
		pointLight(vec3(1.0, 1.0, .0), 10.0)
	};


	const int window_width = 1000;
	const int window_height = 1000;


	WNDCLASSEXA wc = {};
	wc.cbSize = sizeof(wc);
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpfnWndProc = Win32DefaultProc;
	wc.hInstance = hInstance;
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpszClassName = "minimalWindowClass";
	if (!RegisterClassExA(&wc)) { return -1; }
	DWORD window_style = (WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX));
	RECT rc = { 0, 0, window_width, window_height };
	if (!AdjustWindowRect(&rc, window_style, FALSE)) { return -2; }
	HWND window_handle = CreateWindowExA(0, wc.lpszClassName,
		window_title, window_style, CW_USEDEFAULT, CW_USEDEFAULT,
		rc.right - rc.left, rc.bottom - rc.top, 0, 0, hInstance, NULL);
	if (!window_handle) { return 0; }
	ShowWindow(window_handle, nShowCode);
	HDC device = GetDC(window_handle);
	RenderTarget rendertarget(device, window_width, window_height);
	auto prev = get_ticks64();
	while (true) 
	{
		MSG msg = {};
		while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) { return 0; }
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		auto curr = get_ticks64();
		rendertarget.clear(make_color(0x00, 0x00, 0x00, 0xff));

		{
			auto diff = curr - prev;
			prev = curr;

			char title[256] = {};
			sprintf_s(title, sizeof(title), "%s [%dus]", window_title, (int)(diff));
			SetWindowTextA(window_handle, title);
		}

		//for every pixel
		for (size_t y = 0; y < window_width; y++)
		{
			for (size_t x = 0; x < window_height; x++)
			{
				const double u = -.5 + (double)x / (double)window_width;
				const double v = -.5 + (double)y / (double)window_height;

				//send ray through the scene
				ray currentRay(vec3(u, v, .0), vec3::normalize(vec3(u, v, -1.0)));

				vec3 col = calcLighting(currentRay, lights, spheres);
				
				rendertarget.pixel(x, y, make_color(col.r, col.g, col.b, 0xff));
			}
		}
		rendertarget.present();
	}


	return 0;
}