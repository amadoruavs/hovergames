from quart import Quart, render_template, websocket
import requests
import math
import mavsdk
import os
import asyncio
from mavsdk.follow_me import TargetLocation, Config

app = Quart(__name__)
drone = mavsdk.System()
home = None
target = None
drone_position = None
drone_heading = None

EARTH_RADIUS_KM = 6371.0
EARTH_RADIUS_METERS = EARTH_RADIUS_KM * 1000
PI = math.pi

def haversine_distance(lat1, lon1, lat2, lon2):
    lat1, lon1, lat2, lon2 = map(math.radians, [lat1, lon1, lat2, lon2])
    delta_lon = lon2 - lon1
    delta_lat = lat2 - lat1

    a = math.sin(delta_lat / 2) ** 2 + math.cos(lat1) * math.cos(lat2) * math.sin(delta_lon / 2) ** 2
    c = 2 * math.atan2(math.sqrt(a), math.sqrt(1 - a))

    distance = EARTH_RADIUS_METERS * c

    return distance


@app.route("/")
async def hello():
    return "Server OK."

@app.route("/location")
async def location():
    global drone_position
    global drone_heading
    return quart.jsonify({
        "lat": drone_position.latitude,
        "lon": drone_position.longitude,
        "heading": drone_heading,
    })

@app.route("/set_target/<lat>,<lon>")
async def set_target(lat, lon):
    global drone
    global target
    target = TargetLocation(lat, lon, 0, 0, 0, 0)
    await drone.follow_me.set_target_location(target)
    return "OK"

@app.route("/set_home/<lat>,<lon>")
async def set_home(lat, lon):
    global drone
    home = TargetLocation(lat, lon, 0, 0, 0, 0)
    await drone.follow_me.set_target_location(target)
    return "OK"

@app.route("/arm")
async def arm():
    global home
    if home is None:
        return 400, "Home not configured"

    await drone.action.arm()
    await drone.action.takeoff()
    conf = Config(default_height, follow_distance, direction, responsiveness)

    await drone.follow_me.set_config(conf)
    await drone.follow_me.start()
    await drone.follow_me.set_target_location(home)

    return "OK"

@app.route("/land")
async def land():
    await drone.follow_me.stop()
    await drone.action.land()

async def telem_trigger(drone):
    global target
    global home
    global drone
    global drone_position
    async for position in drone.telemetry.position():
        drone_position = position

        if target is None:
            continue

        if haversine_distance(position.latitude_deg, position.longitude_deg,
                              target.latitude_deg, target.longitude_deg) < 5:
            # Trigger
            requests.get("http://192.168.7.173:5000/play")
            target = None
            await drone.follow_me.set_target_location(home)

async def heading_trigger(drone):
    global drone
    global drone_heading
    async for angle in drone.telemetry.attitude_euler():
        drone_heading = angle.yaw_deg

async def main():
    global drone
    global app
    await drone.connect(system_address="serial:///ttyACM0")

    default_height = 8
    follow_distance = 2
    direction = Config.FollowDirection.FRONT
    responsiveness = 0.05

    asyncio.ensure_future(telem_trigger(drone))
    asyncio.ensure_future(app.run_task())

if __name__ == "__main__":
    loop = asyncio.get_event_loop()
    loop.run_until_complete(main())
