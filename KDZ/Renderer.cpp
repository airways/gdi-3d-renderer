#include "Renderer.h"
#include "Matrix.h"

#include <climits>
#include <iostream>
#include <fstream>

namespace GL {

	using namespace System;
	using namespace System::Drawing;
	using namespace System::Collections::Generic;

	Renderer::Renderer(Graphics ^im, int viewportWidth, int viewportHeight) : graphics(im)
	{
		bgColor = Color::White;
		wfBrush = gcnew SolidBrush(Color::Black);
		selectedBrush = gcnew SolidBrush(Color::Yellow);
		surfaceBrush = gcnew SolidBrush(Color::LightBlue);
		// initialize z-buffer
		zbuffer = gcnew array<float, 2>(viewportWidth, viewportHeight);
		setViewport(viewportWidth, viewportHeight);
	}

	void Renderer::setViewport(int width, int height) {
		viewportX = width;
		viewportY = height;
	}

	float Renderer::getViewportAspect() {
		return (float)viewportX / (float)viewportY;
	}

	void Renderer::drawAxes(Matrix4 transformMatrix, bool grid) {
		// TODO: rework
		// get correct axes' coordinates 
		Vector3 origin = NDCtoViewport((transformMatrix * Vector4(0.0f, 0.0f, 0.0f, 1.0f)).fromHomogeneous());
		Vector3 x = NDCtoViewport((transformMatrix * Vector4(50.0f, 0.0f, 0.0f, 1.0f)).fromHomogeneous());
		Vector3 y = NDCtoViewport((transformMatrix * Vector4(0.0f, 50.0f, 0.0f, 1.0f)).fromHomogeneous());
		Vector3 z = NDCtoViewport((transformMatrix * Vector4(0.0f, 0.0f, 50.0f, 1.0f)).fromHomogeneous());
		drawLine(origin, x);
		drawLine(origin, y);
		drawLine(origin, z);
	}

	void Renderer::clearScreen() {
		graphics->Clear(bgColor);
	}

	void Renderer::clearZBuffer() {
		for (int x = 0; x < zbuffer->GetLength(0); x++) {
			for (int y = 0; y < zbuffer->GetLength(1); y++) {
				zbuffer->SetValue(-INFINITY, x, y);
			}
		}
	}


	void Renderer::renderObject(const SceneObject &obj, const Matrix4& transformMatrix, bool wireframe, bool solid) {
		int i = 0;
		for (GL::Polygon pol : obj.polygons) {
			GL::Polygon transformed = pol.getTransformed(transformMatrix);
			Vector3 first = NDCtoViewport(transformed.vertices[0].fromHomogeneous());
			Vector3 second = NDCtoViewport(transformed.vertices[1].fromHomogeneous());
			Vector3 third = NDCtoViewport(transformed.vertices[2].fromHomogeneous());
			if (wireframe) drawPolygon(first, second, third);
			if (solid) {
				// TODO: lighting
				if (i % 2) surfaceBrush->Color = Color::Yellow;
				else surfaceBrush->Color = Color::Blue;
				i++;
				fillPolygon(first, second, third, pol.normals[0], surfaceBrush);
			}
		}
		ztofile();
	}

	void Renderer::ztofile() {
			std::ofstream myfile;
			myfile.open("log.txt");
			for (int x = viewportX / 3; x < 2 * viewportX / 3; x++) {
				for (int y = viewportY / 3; y < 2 * viewportY / 3; y++) {
					if (zbuffer[x, y] == INFINITY) {
						myfile << "| |";
					} else {
						myfile << "|" << zbuffer[x, y] << "|";
					}
				}
				myfile << std::endl;
			}
			myfile.close();
	}

	void Renderer::setGraphics(Graphics ^g) {
		graphics = g;
	}

	Color Renderer::getBGColor() {
		return bgColor;
	}

	Color Renderer::getWFColor() {
		return wfBrush->Color;
	}

	Color Renderer::getSelectedColor() {
		return selectedBrush->Color;
	}

	void Renderer::setBGColor(Color _col) {
		bgColor = _col;
	}

	void Renderer::setWFColor(Color _col) {
		wfBrush->Color = _col;
	}

	void Renderer::setSelectedColor(Color _col) {
		selectedBrush->Color = _col;
	}

	//int outCode(int x, int y, int X1, int X2, int Y1, int Y2) {
	//	int code = 0;
	//	if (x < X1) code |= 0x01;
	//	if (x < X2) code |= 0x02;
	//	if (x < Y1) code |= 0x04;
	//	if (x < Y2) code |= 0x08;
	//	return code;
	//}

	// returns a signum of x
	inline int sign(int x) {
		return (x > 0) - (x < 0);
	}

	// swaps two numbers in-place
	template<typename T>
	void swap(T &x, T &y) {
		T t = x; x = y; y = t;
	}

	float getZ(float x, float y, const Vector3 &p, const Vector3 &normal) {
		return p.z - (normal.x * (x - p.x) + normal.y * (y - p.y)) / normal.z;
	}

	// Draws a line using the Bresenham's algorithm with Z-testing.
	void Renderer::drawLine(const Vector3 &from, const Vector3 &to) {
		int x = from.x, y = from.y;

		int dx = abs(to.x - x);
		int dy = abs(to.y - y);
		int sx = sign(to.x - x);
		int sy = sign(to.y - y);

		// swap the deltas if 2, 3, 6 or 7th octant
		bool isSwap = dy > dx;
		if (isSwap) swap(dx, dy);

		int e = 2 * dy - dx;

		// start drawing
		for (int i = 1; i <= dx; i++, e += 2 * dy) {
			// calculate z-value in pixel
			float t = (Vector3(x, y, 0.f) - Vector3(from.x, from.y, 0.f)).len / (Vector3(to.x, to.y, 0.f) - Vector3(from.x, from.y, 0.f)).len;
			float z = (1 - t) * from.z + t * to.z;
			drawPoint(x, y, z, wfBrush);

			// determine if need to change the direction
			while (e >= 0) {
				if (isSwap) { x += sx; }
				else { y += sy; }
				e -= 2 * dx;
			}

			// increment y or x each step, depending on the octant
			if (isSwap) { y += sy; }
			else { x += sx; }
		}
	}

	void Renderer::drawPoint(int x, int y, float z, SolidBrush ^br) {
		if (x > 0 && x < viewportX && y > 0 && y < viewportY) {
			// z test
			if (1.f / z > zbuffer[x, y]) {
				zbuffer[x, y] = 1.f / z;
				graphics->FillRectangle(br, x, y, 2, 2);
			}
		}
	}

	void Renderer::drawPolygon(const Vector3 &first, const Vector3 &second, const Vector3 &third) {
		drawLine(first, second);
		drawLine(second, third);
		drawLine(third, first);
	}

	float max(float a, float b) { return a > b ? a : b; }
	float min(float a, float b) { return a < b ? a : b; }

	void Renderer::fillPolygon(const Vector3 &_first, const Vector3 &_second, const Vector3 &_third, const Vector3 &normal, SolidBrush ^br) {
		// deformed triangles not needed to be rendered
		if (_first.y == _second.y && _first.y == _third.y) return;
		// copy vectors first
		Vector3 first(_first);
		Vector3 second(_second);
		Vector3 third(_third);
		// sort the vertices
		if (first.y > second.y) swap(first, second);
		if (first.y > third.y) swap(first, third);
		if (second.y > third.y) swap(second, third);
		Vector3 zs(first.z, second.z, third.z);
		int totalHeight = third.y - first.y;
		for (int i = 0; i < totalHeight; i++) {
			bool secondHalf = i > second.y - first.y || second.y == first.y;
			int segmentHeight = secondHalf ? third.y - second.y : second.y - first.y;
			float alpha = (float)i / totalHeight;
			float beta = (float)(i - (secondHalf ? second.y - first.y : 0)) / segmentHeight;
			Vector3 A = first + (third - first) * alpha;
			Vector3 B = secondHalf ? second + (third - second) * beta : first + (second - first) * beta;
			if (A.x > B.x) swap(A, B);
			for (int j = A.x; j <= B.x; j++) {
				Vector3 coordinates = Util::barycentric(Vector3(j, first.y + i, 0.f), _first, _second, _third);
				float z = coordinates.dot(zs);
				drawPoint(j, first.y + i, z, br);
			}
		}
	}

	// Remaps the coordinates from [-1, 1] to the [0, viewport] space. 
	Vector3 Renderer::NDCtoViewport(const Vector3 &vertex) {
		return Vector3((1.0f + vertex.x) * viewportX / 2.0f, (1.0f - vertex.y) * viewportY / 2.0f, vertex.z);
	}
}