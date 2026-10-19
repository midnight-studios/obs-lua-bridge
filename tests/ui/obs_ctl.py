"""obs-websocket helper for tests/ui/script_docks.ps1.

  py tests/ui/obs_ctl.py collection <name>   switch the scene collection
  py tests/ui/obs_ctl.py profile <name>      switch the profile
  py tests/ui/obs_ctl.py owners              print the registered owner ids, comma-separated

Connection settings as in tests/websocket/test_vendor.py (OBS_WS_HOST, OBS_WS_PORT,
OBS_WS_PASSWORD).
"""

import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "websocket"))

import obsws_python as obsws  # noqa: E402
from test_vendor import connect_error, connect_kwargs  # noqa: E402


def main():
    try:
        client = obsws.ReqClient(**{**connect_kwargs(), "timeout": 20})
    except Exception as exc:  # noqa: BLE001
        sys.exit(connect_error(exc))
    try:
        command = sys.argv[1]
        if command == "collection":
            client.set_current_scene_collection(sys.argv[2])
        elif command == "profile":
            client.set_current_profile(sys.argv[2])
        elif command == "owners":
            data = client.call_vendor_request("LuaBridge", "ListOwners", {}).response_data
            print(",".join(o["owner"] for o in data["owners"]))
        else:
            sys.exit(f"unknown command {command}")
    finally:
        client.disconnect()


if __name__ == "__main__":
    main()
