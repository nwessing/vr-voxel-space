float clamp(float value, float min, float max) {
  if (min > value) { return min; }
  if (max < value) { return max; }
  return value;
}
 
