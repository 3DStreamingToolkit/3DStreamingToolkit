package microsoft.a3dtoolkitandroid.util;

//  This source was inspired by http://glmatrix.net/docs/mat4.js.html and the
//  following notice / credit is due..

/* Copyright (c) 2015, Brandon Jones, Colin MacKenzie IV.
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE. */

import Jama.Matrix;

public class MatrixMath {
    public Matrix MatrixCreateIdentity() {
        return Matrix.identity(4,4);
    }

    public Matrix MatrixClone(Matrix original) {
        return (Matrix) original.clone();
    }

    public Matrix MatrixMultiply(Matrix a, Matrix b) {
        return a.times(b);
    }

    public Matrix MatrixTranslate(double[] v) {
        double[][] output = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 }, { v[0], v[1], v[2], 1 } };
        return new Matrix(output);
    }

    public Matrix MatrixRotateX(double radians) {
        double s = (float) Math.sin(radians);
        double c = (float) Math.cos(radians);

        double[][] output = { { 1, 0, 0, 0 }, { 0, c, s, 0 }, { 0, -s, c, 0 }, { 0, 0, 0, 1 } };
        return new Matrix(output);
    }

    public Matrix MatrixRotateY(double radians) {
        double s = (float) Math.sin(radians);
        double c = (float) Math.cos(radians);

        double[][] output = { { c, 0, -s, 0 }, { 0, 1, 0, 0 }, { s, 0, c, 0 }, { 0, 0, 0, 1 } };
        return new Matrix(output);
    }

    public Matrix MatrixRotateZ(double radians) {
        double s = (float) Math.sin(radians);
        double c = (float) Math.cos(radians);

        double[][] output = { { c, s, 0, 0 }, { -s, c, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 } };
        return new Matrix(output);
    }
}
