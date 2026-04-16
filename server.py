from flask import Flask, request, jsonify
import requests
import math

app = Flask(__name__)

API_KEY = "97d77f2d7fe34fba932422c7fcebd752"

# ================= DISTANCE =================
def distance(lat1, lon1, lat2, lon2):
    R = 6371
    dlat = math.radians(lat2 - lat1)
    dlon = math.radians(lon2 - lon1)

    a = (math.sin(dlat/2)**2 +
         math.cos(math.radians(lat1)) *
         math.cos(math.radians(lat2)) *
         math.sin(dlon/2)**2)

    return 2 * R * math.atan2(math.sqrt(a), math.sqrt(1-a))


# ================= GEOAPIFY =================
def fetch_geoapify(lat, lon):
    url = f"https://api.geoapify.com/v2/places?categories=healthcare.hospital&filter=circle:{lon},{lat},20000&bias=proximity:{lon},{lat}&limit=10&apiKey={API_KEY}"

    try:
        r = requests.get(url, timeout=10)
        if r.status_code == 200:
            return r.json()
    except:
        pass

    return None


# ================= OVERPASS =================
def fetch_overpass(lat, lon):
    url = "https://overpass-api.de/api/interpreter"

    query = f"""
    [out:json];
    (
      node["amenity"="hospital"](around:20000,{lat},{lon});
      way["amenity"="hospital"](around:20000,{lat},{lon});
      relation["amenity"="hospital"](around:20000,{lat},{lon});
    );
    out center;
    """

    try:
        r = requests.post(url, data={'data': query}, timeout=15)
        if r.status_code == 200:
            return r.json()
    except:
        pass

    return None


# ================= PROCESS =================
def process_data(elements, lat, lon):

    nearest = None
    min_d = float("inf")

    for h in elements:

        # Geoapify
        if "properties" in h:
            prop = h["properties"]
            coords = h.get("geometry", {}).get("coordinates", [])

            if len(coords) < 2:
                continue

            hlon, hlat = coords[0], coords[1]

        # Overpass
        else:
            prop = h.get("tags", {})
            hlat = h.get("lat") or (h.get("center") or {}).get("lat")
            hlon = h.get("lon") or (h.get("center") or {}).get("lon")

            if hlat is None or hlon is None:
                continue

        name = prop.get("name") or "Hospital"

        d = distance(lat, lon, float(hlat), float(hlon))

        if d < min_d:
            min_d = d
            nearest = {
                "name": name,
                "lat": float(hlat),
                "lon": float(hlon),
                "distance_km": round(min_d, 2)
            }

    return nearest


# ================= MAIN =================
def get_hospital(lat, lon):

    data = fetch_geoapify(lat, lon)

    elements = []

    if data and "features" in data:
        elements = data["features"]

    if not elements:
        data = fetch_overpass(lat, lon)
        if data:
            elements = data.get("elements", [])

    result = process_data(elements, lat, lon)

    if result:
        result["map"] = f"https://www.google.com/maps?q={result['lat']},{result['lon']}"
        return result

    return {
        "name": "Fallback Location",
        "lat": lat,
        "lon": lon,
        "distance_km": 0,
        "map": f"https://www.google.com/maps?q={lat},{lon}"
    }


# ================= ROUTES =================
@app.route('/')
def home():
    return "🚑 Hospital Finder Running"

@app.route('/data')
def data():

    try:
        lat = float(request.args.get('lat'))
        lon = float(request.args.get('lon'))
    except:
        return jsonify({"error": "Invalid input"}), 400

    return jsonify(get_hospital(lat, lon))


# ================= RUN =================
if __name__ == "__main__":
    print("🚀 Server Started")
    app.run(host="0.0.0.0", port=5000, debug=True)