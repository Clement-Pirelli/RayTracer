#include <cmath>
#include <fstream>
#include <chrono>
#include <iostream>
#include <string>

//CONSTANTS
const double RENDER_DISTANCE = 9999.0;

#define SAFETY_CLAMP_IMG


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

const intersection intersection::MISS = intersection(RENDER_DISTANCE, vec3(.0), vec3(.0), material(.0));

struct image
{
	unsigned int width;
	unsigned int height;
	unsigned char *img;

	image(unsigned int givenWidth, unsigned int givenHeight) : width(givenWidth), height(givenHeight) 
	{
		img = new unsigned char[3 * width*height];
	}
	~image()
	{
		delete[] img;
	}
	void writeToPixel(unsigned int x, unsigned int y, vec3 val)
	{

#ifdef SAFETY_CLAMP_IMG
		val = clamp(val, .0, 255.0);
#endif // SAFETY_CLAMP_IMG
		
		img[(x + y * width) * 3 + 2] = (unsigned char)(val.r);
		img[(x + y * width) * 3 + 1] = (unsigned char)(val.g);
		img[(x + y * width) * 3 + 0] = (unsigned char)(val.b);
	}

	//original code from https://stackoverflow.com/a/2654860
	void writeDataAt(const char * path)
	{
		FILE *f;
		int filesize = 54 + 3 * width*height;  //w is your image width, h is image height, both int

		unsigned char bmpfileheader[14] = { 'B','M', 0,0,0,0, 0,0, 0,0, 54,0,0,0 };
		unsigned char bmpinfoheader[40] = { 40,0,0,0, 0,0,0,0, 0,0,0,0, 1,0, 24,0 };
		unsigned char bmppad[3] = { 0,0,0 };

		bmpfileheader[2] = (unsigned char)(filesize);
		bmpfileheader[3] = (unsigned char)(filesize >> 8);
		bmpfileheader[4] = (unsigned char)(filesize >> 16);
		bmpfileheader[5] = (unsigned char)(filesize >> 24);

		bmpinfoheader[4] = (unsigned char)(width);
		bmpinfoheader[5] = (unsigned char)(width >> 8);
		bmpinfoheader[6] = (unsigned char)(width >> 16);
		bmpinfoheader[7] = (unsigned char)(width >> 24);
		bmpinfoheader[8] = (unsigned char)(height);
		bmpinfoheader[9] = (unsigned char)(height >> 8);
		bmpinfoheader[10] = (unsigned char)(height >> 16);
		bmpinfoheader[11] = (unsigned char)(height >> 24);

		fopen_s(&f, path, "wb");
		fwrite(bmpfileheader, 1, 14, f);
		fwrite(bmpinfoheader, 1, 40, f);
		for (unsigned int i = 0; i < height; i++)
		{
			fwrite(img + (width*(height - i - 1) * 3), 3, width, f);
			fwrite(bmppad, 1, (4 - (width * 3) % 4) % 4, f);
		}

		free(img);
		fclose(f);
	}
};

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

static inline intersection intersect(ray &pRay, sphere &pSphere)
{
	double radius2 = pSphere.radius*pSphere.radius;
	vec3 L = pSphere.origin - pRay.origin;
	double tca = vec3::dot(L, pRay.direction);
	double d2 = vec3::dot(L, L) - tca * tca;
	if (d2 > radius2) return intersection::MISS;
	double thc = sqrt(radius2 - d2);
	double t0 = tca - thc;
	double t1 = tca + thc;
	double t = min(t0, t1);

	intersection hit = intersection(t, pSphere.color, (pSphere.origin - (pRay.origin + pRay.direction*t)) / pSphere.radius, pSphere.mat);
	return mix(intersection::MISS, hit, (double)(t < .0));
}

const double ambientLight = .0;
const unsigned int sphereCount = 2;
const unsigned int maxBounces = 100;


int main()
{
	//screen params
	const unsigned int SCREEN_WIDTH = 1000;
	const unsigned int SCREEN_HEIGHT = 1000;

	std::chrono::milliseconds time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());

	image img(SCREEN_WIDTH, SCREEN_HEIGHT);

	sphere spheres[sphereCount]
	{
		sphere(vec3(-.5, -.4, 10.0), vec3(.0, .0, 255.0), .6, material(.9, .1, 64.0)),
		sphere(vec3(1.0, .5, 13.0), vec3(255.0, .0, 255.0), 1.0, material(.5, .9, 8.0))
	};

	pointLight l(vec3(.0, 7.0, 0.0), 200.0);
	
	//for every pixel
	for (size_t y = 0; y < SCREEN_WIDTH; y++)
	{
		for (size_t x = 0; x < SCREEN_HEIGHT; x++)
		{
			double u =  -.5 + (double)x / (double)SCREEN_WIDTH;
			double v =  -.5 + (double)y / (double)SCREEN_HEIGHT;

			intersection finalIntersection = intersection::MISS;

			//send ray through the scene
			ray currentRay(vec3(u, v, .0), vec3::normalize(vec3(u,v,-1.0)));
			

			vec3 finalColor = vec3(.0);
			//for every sphere
			for(unsigned int i = 0; i < sphereCount; i++)
			{
				intersection currentIntersection = intersect(currentRay, spheres[i]);
				finalIntersection = mix(finalIntersection, currentIntersection, (double)(currentIntersection.dist < finalIntersection.dist));
			}

			vec3 color = vec3(.0);
			vec3 intersectionPoint = currentRay.origin + currentRay.direction * finalIntersection.dist;
			vec3 lightDir = l.origin - intersectionPoint;
			double distance = vec3::magnitude(lightDir);
			double lightIntensity = l.intensity / (distance*distance);
			double diffuseTerm = clamp(vec3::dot(lightDir/distance, finalIntersection.normal), .0, 1.0) * finalIntersection.mat.diff;


			vec3 reflection = finalIntersection.normal * 2.0 * vec3::dot(finalIntersection.normal, lightDir) - lightDir;

			//specular term
			//Thanks to UglySwedishFish#3207 on discord for their help with this :
			//dot products range from -1 to 1, so the dot product of the reflection and the ray's direction
			//has to be clamped so that a negative value doesn't get squared into a positive value
			double specularTerm = pow(clamp(vec3::dot(vec3::normalize(reflection), currentRay.direction), .0, 1.0), finalIntersection.mat.gloss) * finalIntersection.mat.spec;
			color += clamp(finalIntersection.color * lightIntensity * (diffuseTerm + specularTerm + ambientLight), .0, 255.0);
			finalColor += color;
			
			//lights' aura
			{
				//distance from the ray's origin to the light's origin
				double distanceToLightOrigin = vec3::magnitude(l.origin - currentRay.origin);
			
				//projection of the ray on the light, aka the point on the ray which is the closest to the light's origin
				vec3 rayProjection = currentRay.origin + currentRay.direction * distanceToLightOrigin;
			
				finalColor += l.intensity / (vec3::magnitude(l.origin - rayProjection)*255.0);
			}
			
			img.writeToPixel(x, y, finalColor);
		}
	}
	time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()) - time;
	std::cout << "Raytracing done in : " << time.count() << " milliseconds\n";

	std::string filename = "spheres_" + std::to_string(time.count()) + ".bmp";
	img.writeDataAt(filename.c_str());
	
	int input = 0;
	std::cin >> input;

	return 0;
}