export function cloneMatrix(a) {
  let m = createIdentityMatrix();
  m[0] = a[0], m[1] = a[1], m[2] = a[2], m[3] = a[3];
  m[4] = a[4], m[5] = a[5], m[6] = a[6], m[7] = a[7];
  m[8] = a[8], m[9] = a[9], m[10] = a[10], m[11] = a[11];
  m[12] = a[12], m[13] = a[13], m[14] = a[14], m[15] = a[15];
  return m;
}

export function createIdentityMatrix() {
  return [ 1.0, 0.0, 0.0, 0.0, 
           0.0, 1.0, 0.0, 0.0,
           0.0, 0.0, 1.0, 0.0,
           0.0, 0.0, 0.0, 1.0 ];
}

export function createTranslationMatrix(v) {
  let m = createIdentityMatrix();
  m[12] = v[0], m[13] = v[1], m[14] = v[2];
  return m;
}

export function createRotateYMatrix(rad) {
  let s = Math.sin(rad);
  let c = Math.cos(rad);
  
  return [ c, 0.0, -s, 0.0,
           0.0, 1.0, 0.0, 0.0,
           s, 0.0, c, 0.0,
           0.0, 0.0, 0.0, 1.0 ];
}

export function createRotateZMatrix(rad) {
  let s = Math.sin(rad);
  let c = Math.cos(rad);
  
  return [ c, s, 0.0, 0.0,
           -s, c, 0.0, 0.0,
           0.0, 0.0, 1.0, 0.0,
           0.0, 0.0, 0.0, 1.0 ];
}

export function matrixMultiply(a, b) {
  let m = createIdentityMatrix();

  let a00 = a[0], a01 = a[1], a02 = a[2], a03 = a[3];
  let a10 = a[4], a11 = a[5], a12 = a[6], a13 = a[7];
  let a20 = a[8], a21 = a[9], a22 = a[10], a23 = a[11];
  let a30 = a[12], a31 = a[13], a32 = a[14], a33 = a[15];

  let b0  = b[0], b1 = b[1], b2 = b[2], b3 = b[3];
  m[0] = b0*a00 + b1*a10 + b2*a20 + b3*a30;
  m[1] = b0*a01 + b1*a11 + b2*a21 + b3*a31;
  m[2] = b0*a02 + b1*a12 + b2*a22 + b3*a32;
  m[3] = b0*a03 + b1*a13 + b2*a23 + b3*a33;

  b0 = b[4]; b1 = b[5]; b2 = b[6]; b3 = b[7];
  m[4] = b0*a00 + b1*a10 + b2*a20 + b3*a30;
  m[5] = b0*a01 + b1*a11 + b2*a21 + b3*a31;
  m[6] = b0*a02 + b1*a12 + b2*a22 + b3*a32;
  m[7] = b0*a03 + b1*a13 + b2*a23 + b3*a33;

  b0 = b[8]; b1 = b[9]; b2 = b[10]; b3 = b[11];
  m[8] = b0*a00 + b1*a10 + b2*a20 + b3*a30;
  m[9] = b0*a01 + b1*a11 + b2*a21 + b3*a31;
  m[10] = b0*a02 + b1*a12 + b2*a22 + b3*a32;
  m[11] = b0*a03 + b1*a13 + b2*a23 + b3*a33;

  b0 = b[12]; b1 = b[13]; b2 = b[14]; b3 = b[15];
  m[12] = b0*a00 + b1*a10 + b2*a20 + b3*a30;
  m[13] = b0*a01 + b1*a11 + b2*a21 + b3*a31;
  m[14] = b0*a02 + b1*a12 + b2*a22 + b3*a32;
  m[15] = b0*a03 + b1*a13 + b2*a23 + b3*a33;
  
  return m;
}
