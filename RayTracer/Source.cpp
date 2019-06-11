#include <cmath>
#include <thread>

//CONSTANTS
const double RENDER_DISTANCE = 9999.0;
const double SMALLEST_DISTANCE = .000001;

#pragma region SCALAR OPERATIONS

static inline double min(double a, double b)
{
	return (a > b) ? b : a;
}

static inline double max(double a, double b)
{
	return (a > b) ? a : b;
}

static inline double clamp(double giventerm, double givenmin, double givenmax)
{
	giventerm = min(giventerm, givenmax);
	giventerm = max(giventerm, givenmin);

	return giventerm;
}

#pragma endregion

#pragma region VECTOR3

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
	vec3 operator -(const vec3 b) const { return vec3(x - b.x, y - b.y, z - b.z); }
	vec3 operator +(vec3 b){ return vec3(x + b.x, y + b.y, z + b.z); }
	vec3 operator /(double b){ return vec3(x / b, y / b, z / b); }
	vec3 operator /(const double b) const { return vec3(x / b, y / b, z / b); }
	vec3 operator *(double b){ return vec3(x * b, y * b, z * b); }
	vec3 operator *(const double b) const { return vec3(x * b, y * b, z * b); }
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

static inline vec3 reflect(vec3 incident, vec3 normal)
{
	return vec3::normalize(incident - normal * 2.0 * vec3::dot(normal, incident));
}

#pragma endregion

#pragma region STRUCTS

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
	double reflect;
	double opac;
	double refract;

	material(double givenParams) { diff = spec = gloss = reflect = opac = refract = givenParams; }
	material(double givenDiff, double givenSpec, double givenGloss, double givenReflect, double givenOpac, double givenRefract) : diff(givenDiff), spec(givenSpec), gloss(givenGloss), reflect(givenReflect), opac(givenOpac), refract(givenRefract){}
};

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

#pragma endregion

#pragma region RAYTRACING

const unsigned int sphereCount = 3;
const unsigned int lightCount = 2;
const double ambientLight = .01;
const unsigned int maxBounces = 4;

static inline double rayTrace(const ray &pRay, const sphere &pSphere)
{
	double radius2 = pSphere.radius*pSphere.radius;
	vec3 L = pSphere.origin - pRay.origin;
	double tca = vec3::dot(L, pRay.direction);
	double d2 = vec3::dot(L, L) - tca * tca;
	if (d2 > radius2) return intersection::MISS.dist;
	double thc = sqrt(radius2 - d2);
	double t0 = tca - thc;
	double t1 = tca + thc;
	return min(t0, t1);
}

static inline intersection intersect(ray &pRay, const sphere &pSphere)
{
	double t = rayTrace(pRay, pSphere);
	intersection hit = intersection(t, pSphere.color, (pSphere.origin - (pRay.origin + pRay.direction*t)) / pSphere.radius, pSphere.mat);
	if (t > SMALLEST_DISTANCE) return hit;
	return intersection::MISS;
}

static inline vec3 calcLighting(const ray &pRay, const vec3 &intersectionPoint, const pointLight &pLight, const intersection &pIntersection)
{
	const vec3 lightDir = intersectionPoint - pLight.origin;
	const double lightDistance = vec3::magnitude(lightDir);
	const double lightIntensity = pLight.intensity / (lightDistance*lightDistance);
	const double diffuseTerm = clamp(vec3::dot(lightDir / lightDistance, pIntersection.normal), .0, 1.0) * pIntersection.mat.diff;
	const vec3 reflection = reflect(lightDir, pIntersection.normal);
	const double specularTerm = pow(clamp(vec3::dot(vec3::normalize(reflection), pRay.direction), .0, 1.0), pIntersection.mat.gloss) * pIntersection.mat.spec;
 
	return pIntersection.color * lightIntensity * (diffuseTerm + specularTerm + ambientLight);
}

static inline bool isInShadow(const ray &pRay, const sphere &pSphere, const vec3 &lightPos, const double minDistance)
{
	const double t = rayTrace(pRay, pSphere);
	return (t > minDistance) && (t < vec3::magnitude(lightPos - pRay.origin));
}

static inline intersection bounce(ray &pRay, vec3 &outColor, const pointLight *pLights, const sphere *pSpheres)
{
	vec3 finalColor = vec3(.0);
	intersection finalIntersection = intersection::MISS;
	//for every sphere
	for (unsigned int sphereIndex = 0; sphereIndex < sphereCount; sphereIndex++)
	{
		intersection currentIntersection = intersect(pRay, pSpheres[sphereIndex]);
		if (currentIntersection.dist < finalIntersection.dist) finalIntersection = currentIntersection;
	}
	vec3 intersectionPoint = pRay.origin + pRay.direction * finalIntersection.dist;

	for (unsigned int lightIndex = 0; lightIndex < lightCount; lightIndex++)
	{
		bool inShadow = false;
		for (unsigned int sphereIndex = 0; sphereIndex < sphereCount; sphereIndex++)
		{
			ray shadowRay = ray(intersectionPoint, vec3::normalize(pLights[lightIndex].origin - intersectionPoint));
			inShadow = inShadow || isInShadow(shadowRay, pSpheres[sphereIndex], pLights[lightIndex].origin, .00001);
		}
		if (inShadow) continue;
		finalColor += calcLighting(pRay, intersectionPoint, pLights[lightIndex], finalIntersection);
	}

	finalColor = clamp(finalColor, .0, 255.0);
	//add background color
	if (finalIntersection.dist == intersection::MISS.dist) finalColor = intersection::MISS.color;
	outColor = finalColor;
	pRay.origin = pRay.origin + pRay.direction * finalIntersection.dist;
	pRay.direction = reflect(pRay.direction, finalIntersection.normal);
	return finalIntersection;
}

static inline vec3 traceScene(ray &pRay, const pointLight *pLights, const sphere *pSpheres)
{
	vec3 finalColor = vec3(.0);
	double reflectance = 1.0;
	bool bounceMissed = false;
	unsigned int bounceIndex = 1;

	while ((!bounceMissed && bounceIndex <= maxBounces && reflectance > .0))
	{
		vec3 bounceColor = vec3(.0);
		intersection bounceIntersection = bounce(pRay, bounceColor, pLights, pSpheres);
		bounceColor *= reflectance;
		finalColor += clamp(bounceColor, .0, 255.0);
		reflectance -= 1.0 - bounceIntersection.mat.reflect;
		bounceIndex++;
		bounceMissed = bounceMissed || (bounceIntersection.dist == intersection::MISS.dist);
	}
	return clamp(finalColor, .0, 255.0);
}

#pragma endregion

#pragma region WINDOW DISPLAY

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

#pragma endregion

static inline void renderPixels(RenderTarget &rt, double xStart, double threadWidth, double yStart, double threadHeight, double width, double height)
{
	const sphere spheres[sphereCount]
	{
		sphere(vec3(.0, .5, 2.0),		vec3(255.0, 255.0, 255.0),	.4,	material(.4, .6, 64.0, .3, 1.0, 1.0)),
		sphere(vec3(-.3, -.4, 2.1),		vec3(255.0, .0, 255.0),		.3,	material(1., .0, 8.0, .2, 1.0, 1.0)),
		sphere(vec3(.3, -.4,2.1),		vec3(20.0, 200.0, 20.0),	.3,	material(.9, .1, 8.0, .9, 1.0, 1.0))
	};

	const pointLight lights[lightCount]
	{
		pointLight(vec3(1.0, .0, .0), 1.5),
		pointLight(vec3(1.0, .0,1.0), 1.0)
	};

	for (double y = yStart; y < yStart + threadWidth; y += 1.0)
	{
		for (double x = xStart; x < xStart + threadHeight; x += 1.0)
		{
			const double u = -.5 + x / width;
			const double v = -.5 + y / height;

			//send ray through the scene
			ray currentRay(vec3(u, v, .0), vec3::normalize(vec3(u, v, 1.0)));

			vec3 col = traceScene(currentRay, lights, spheres);

			rt.pixel(x, y, make_color(col.r, col.g, col.b, 0xff));
		}
	}
}


int CALLBACK
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCode) {
	const char* window_title = "MinimalWindowPixelOnScreen";

	const int window_width = 1024;
	const int window_height = 1024;

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
		static const double width = (double)window_width;
		static const double height = (double)window_height;

		static const unsigned int threadAmount = 4;
		static const double threadWidth = width / threadAmount * 2;
		static const double threadHeight = height / threadAmount * 2;

		std::thread t1(renderPixels, std::ref(rendertarget), .0, threadWidth, .0, threadHeight, width, height);
		std::thread t2(renderPixels, std::ref(rendertarget), threadWidth, threadWidth, .0, threadHeight, width, height);
		std::thread t3(renderPixels, std::ref(rendertarget), .0, threadWidth, threadHeight, threadHeight, width, height);
		std::thread t4(renderPixels, std::ref(rendertarget), threadWidth, threadWidth, threadHeight, threadHeight, width, height);

		t1.join();
		t2.join();
		t3.join();
		t4.join();

		rendertarget.present();
	}

	return 0;
}