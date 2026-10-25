#!/bin/sh
# Points an A record at this network's public IPv4 address, through the
# Cloudflare API. Run it every few minutes (cron or a systemd timer).
#
#   CF_API_TOKEN  API token with Zone:DNS:Edit on the zone (required)
#   CF_ZONE_ID    the zone ID, from the zone's overview page (required)
#   RECORD        the name to update (default pong.luiseel.dev)
#
# The record is DNS only (grey cloud, proxied=false): the Cloudflare proxy
# does not carry the game's UDP. Needs curl and jq.
set -eu

: "${CF_API_TOKEN:?set CF_API_TOKEN}"
: "${CF_ZONE_ID:?set CF_ZONE_ID}"
RECORD=${RECORD:-pong.luiseel.dev}
API="https://api.cloudflare.com/client/v4/zones/$CF_ZONE_ID/dns_records"

api() {
    curl -fsS -H "Authorization: Bearer $CF_API_TOKEN" -H "Content-Type: application/json" "$@"
}

IP=$(curl -fsS https://api.ipify.org)
case "$IP" in
    *[!0-9.]* | "") echo "ddns: bad public IP: $IP" >&2; exit 1 ;;
esac

EXISTING=$(api "$API?type=A&name=$RECORD")
ID=$(echo "$EXISTING" | jq -r '.result[0].id // empty')
CURRENT=$(echo "$EXISTING" | jq -r '.result[0].content // empty')
BODY=$(jq -n --arg name "$RECORD" --arg ip "$IP" \
    '{type: "A", name: $name, content: $ip, ttl: 60, proxied: false}')

if [ -z "$ID" ]; then
    api -X POST "$API" --data "$BODY" >/dev/null
    echo "ddns: $RECORD created at $IP"
elif [ "$CURRENT" != "$IP" ]; then
    api -X PATCH "$API/$ID" --data "$BODY" >/dev/null
    echo "ddns: $RECORD moved from $CURRENT to $IP"
fi
