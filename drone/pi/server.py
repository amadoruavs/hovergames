def distance(camera_tilt, altitude, direction, ground_altitude, cur_lat, cur_lon):
    ground_distance = altitude-ground_altitude
    d = math.tan(radians(camera_tilt))*ground_distance #distance in meters
    R = 6378000.1 #Radius of the Earth
    brng = math.radians(direction) #Bearing is whatever degrees converted to radians.
    lat1 = math.radians(cur_lat) #Current lat point converted to radians
    lon1 = math.radians(cur_lon) #Current long point converted to radians
    lat2 = math.asin( math.sin(lat1)*math.cos((d)/R) +
         math.cos(lat1)*math.sin(d/R)*math.cos(brng))
    lon2 = lon1 + math.atan2(math.sin(brng)*math.sin(d/R)*math.cos(lat1),
                 math.cos(d/R)-math.sin(lat1)*math.sin(lat2))
    lat2 = math.degrees(lat2)
    lon2 = math.degrees(lon2)
    return([lat2, lon2])
