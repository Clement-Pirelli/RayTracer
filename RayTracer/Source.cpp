#include <cmath>
#include <thread>

//CONSTANTS
constexpr double RENDER_DISTANCE = 999.0;
constexpr double SMALLEST_DISTANCE = .0001;

#pragma region SCALAR OPERATIONS

static inline double min(const double a, const double b)
{
	return (a > b) ? b : a;
}

static inline double max(const double a, const double b)
{
	return (a > b) ? a : b;
}

static inline double clamp(const double giventerm, const double givenmin, const double givenmax)
{
	return min(max(giventerm, givenmin), givenmax);
}

static inline double mix(const double a, const double b, const double t)
{
	return a * (1. - t) + b*t;
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

	vec3 operator -(vec3 b) const { return {x - b.x, y - b.y, z - b.z}; }
	vec3 operator +(vec3 b) const { return {x + b.x, y + b.y, z + b.z}; }
	vec3 operator /(double b) const { return {x / b, y / b, z / b}; }
	vec3 operator *(double b) const { return {x * b, y * b, z * b}; }
	vec3 operator *(vec3 b) const { return { x * b.x, y * b.y, z * b.z }; }
	void operator /=(double b){ x /= b; y /= b; z /= b; }
	void operator *=(double b){ x *= b; y *= b; z *= b; }
	void operator +=(vec3 b){ x += b.x; y += b.y; z += b.z; }

	static inline double dot(const vec3 &a, const vec3 &b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
	static inline double magnitude(const vec3 &a){ return sqrt(a.x*a.x + a.y*a.y + a.z*a.z); }

	static inline vec3 normalize(const vec3 &a){ return a / magnitude(a); }
};

inline vec3 clamp(const vec3 &giventerm, const double givenmin, const double givenmax)
{
	return vec3(clamp(giventerm.x, givenmin, givenmax), clamp(giventerm.y, givenmin, givenmax), clamp(giventerm.z, givenmin, givenmax));
}

inline vec3 reflect(const vec3 &incident, const vec3 &normal)
{
	const vec3 I = vec3::normalize(incident);

	return (normal * 2.0 * vec3::dot(normal, I)) - I;
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

constexpr unsigned int sphereCount = 3;
constexpr unsigned int lightCount = 2;
constexpr double ambientLight = .01;
constexpr unsigned int maxBounces = 4;
vec3 cameraPosition(.0,.0,.0);
vec3 cameraDirection(.0,.0,1.0);

static inline double rayTrace(const ray &pRay, const sphere &pSphere)
{
	const double radius2 = pSphere.radius*pSphere.radius;
	const vec3 L = pSphere.origin - pRay.origin;
	const double tca = vec3::dot(L, pRay.direction);
	const double d2 = vec3::dot(L, L) - tca * tca;
	if (d2 > radius2) return intersection::MISS.dist;
	const double thc = sqrt(radius2 - d2);
	return min(tca - thc, tca + thc);
}

static intersection intersect(const ray &pRay, const sphere &pSphere)
{
	double t = rayTrace(pRay, pSphere);
	if (t < SMALLEST_DISTANCE) return intersection::MISS;
	return intersection(t, pSphere.color, (pSphere.origin - (pRay.origin + pRay.direction*t)) / pSphere.radius, pSphere.mat);
}

static inline vec3 calcLighting(const ray &pRay, const vec3 &intersectionPoint, const pointLight &pLight, const intersection &pIntersection)
{
	const vec3 lightDir = intersectionPoint - pLight.origin;
	const double lightDistance = vec3::magnitude(lightDir);
	const double lightIntensity = pLight.intensity / (lightDistance*lightDistance);
	const double diffuseTerm = clamp(vec3::dot(lightDir/lightDistance, pIntersection.normal), .0, 1.0) * pIntersection.mat.diff;
	const vec3 reflection = reflect(lightDir, pIntersection.normal);
	const double specularTerm = pow(clamp(vec3::dot(reflection, cameraDirection), .0, 1.0), pIntersection.mat.gloss) * pIntersection.mat.spec;


	return pIntersection.color * lightIntensity * (diffuseTerm + specularTerm + ambientLight);
}

static inline bool isInShadow(const ray &pRay, const sphere &pSphere, const vec3 &lightPos, const double minDistance)
{
	const double t = rayTrace(pRay, pSphere);
	return (t > minDistance) && (t < vec3::magnitude(lightPos - pRay.origin));
}

static inline intersection bounce(const ray &pRay, vec3 &outColor, const pointLight *pLights, const sphere *pSpheres)
{
	vec3 finalColor = vec3(.0);
	intersection finalIntersection = intersection::MISS;
	//for every sphere
	for (unsigned int sphereIndex = 0; sphereIndex < sphereCount; sphereIndex++)
	{
		intersection currentIntersection = intersect(pRay, pSpheres[sphereIndex]);
		if (currentIntersection.dist < finalIntersection.dist) finalIntersection = currentIntersection;
	}
	
	if (finalIntersection.dist == intersection::MISS.dist) return intersection::MISS;

	vec3 intersectionPoint = pRay.origin + pRay.direction * finalIntersection.dist;

	for (unsigned int lightIndex = 0; lightIndex < lightCount; lightIndex++)
	{
		bool inShadow = false;
		for (unsigned int sphereIndex = 0; sphereIndex < sphereCount; sphereIndex++)
		{
			ray shadowRay = ray(intersectionPoint, vec3::normalize(pLights[lightIndex].origin - intersectionPoint));
			shadowRay.origin += finalIntersection.normal*SMALLEST_DISTANCE;
			inShadow = isInShadow(shadowRay, pSpheres[sphereIndex], pLights[lightIndex].origin, .00001);
			if (inShadow) break;
		}
		if(!inShadow)
			finalColor += calcLighting(pRay, intersectionPoint, pLights[lightIndex], finalIntersection);
	}

	outColor = clamp(finalColor, .0, 255.0);
	return finalIntersection;
}

inline vec3 traceScene(const ray &pRay, const pointLight *pLights, const sphere *pSpheres)
{
	vec3 finalColor = intersection::MISS.color;
	double reflectance = 1.0;
	unsigned int bounceIndex = 1;
	ray currentRay = pRay;

	while (bounceIndex <= maxBounces && reflectance > .01)
	{
		vec3 bounceColor = vec3(.0);
		intersection bounceIntersection = bounce(currentRay, bounceColor, pLights, pSpheres);
		if (bounceIntersection.dist == intersection::MISS.dist) return finalColor;
		currentRay.origin = currentRay.origin + currentRay.direction * bounceIntersection.dist;
		currentRay.direction = reflect(currentRay.direction, bounceIntersection.normal)*-1.0;
		finalColor += bounceColor*reflectance;
		currentRay.origin += currentRay.direction*SMALLEST_DISTANCE;
		reflectance -= 1.0 - bounceIntersection.mat.reflect;
		bounceIndex++;
	}
	return clamp(finalColor, .0, 255.0);
}

#pragma endregion

#pragma region WINDOW DISPLAY

//base code for window display provided Tommi Lipponen
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

double currentTime = .0;

static inline void renderPixels(RenderTarget &rt, double xStart, double threadWidth, double yStart, double threadHeight, double width, double height)
{
	const sphere spheres[sphereCount]
	{
		sphere({.2, cos(currentTime)*-.2, 1.0},		{255.0, 255.0, 255.0},	.1,	material(1.0, .0, 1.0, .2, 1.0, 1.0)),
		sphere({-.3, -.4, 2.5 - cos(currentTime)},	{255.0, .0, 255.0},		.2,	material(1.0, 1.0, 128.0, .5, 1.0, 1.0)),
		sphere({.3, -.4,2.1},						{255.0, .0, .0},		.3,	material(1., 1.0, 64.0, .5, 1.0, 1.0))
	};

	const pointLight lights[lightCount]
	{
		pointLight({-.6, .0, -.7}, .7),
		pointLight({.0, .0,-.4}, 1.0)
	};

	for (double y = yStart; y < yStart + threadWidth; y += 1.0)
	{
		for (double x = xStart; x < xStart + threadHeight; x += 1.0)
		{
			const double u = -.5 + x / width;
			const double v = -.5 + y / height;
			vec3 dir = vec3::normalize({ cameraDirection.x + u, cameraDirection.y + v, cameraDirection.z });
			//send ray through the scene
			ray currentRay(cameraPosition , dir);

			vec3 col = clamp(traceScene(currentRay, lights, spheres),.0,255.0);
			rt.pixel(x, y, make_color(col.r, col.g, col.b, 0xff));
		}
	}
}

const int window_width = 1024;
const int window_height = 1024;
const double width = (double)window_width;
const double height = (double)window_height;

const unsigned int threadX = 3;
const unsigned int threadY = 3;
std::thread threads[threadX * threadY];
const double threadWidth = width / threadX;
const double threadHeight = height / threadY;


int CALLBACK
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCode) {
	const char* window_title = "Raytracer!";

	

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
			sprintf_s(title, sizeof(title), "%s [%dms]", window_title, (int)(diff/1000));
			SetWindowTextA(window_handle, title);
		}

		//for every pixel
		

		currentTime = (double)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()*.001;

		for (int i = 0; i < threadY; i++)
		{
			for(int j = 0; j < threadX; j++)
			{
				new (threads + (i*threadX + j)) std::thread(renderPixels, std::ref(rendertarget), threadWidth*(double)j, threadWidth, threadHeight*(double)i, threadHeight, width, height);
			}
		}

		for(int i = 0; i < threadX*threadY; i++)
		{
			threads[i].join();
		}

		rendertarget.present();
	}

	return 0;
}