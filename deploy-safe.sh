#!/bin/bash

# Safe Netlify Deployment Script
# This ensures both static files AND functions are deployed together
# to prevent the recurring issue of drag & drop overwriting functions

echo "🚀 Starting safe Netlify deployment..."
echo "📁 Current directory: $(pwd)"

# Check if we're in the right directory
if [ ! -f "netlify-deploy/netlify.toml" ]; then
    echo "❌ Error: netlify-deploy/netlify.toml not found"
    echo "Please run this script from the oura-stats-watchface directory"
    exit 1
fi

# Check if netlify CLI is installed
if ! command -v netlify &> /dev/null; then
    echo "❌ Error: Netlify CLI not installed"
    echo "Install with: npm install -g netlify-cli"
    exit 1
fi

# Pre-copy: ensure latest config HTML is present in deploy dir
SRC_CONFIG="pebble-static-config.html"
DST_CONFIG="netlify-deploy/pebble-static-config.html"
if [ -f "$SRC_CONFIG" ]; then
    if [ ! -f "$DST_CONFIG" ] || ! cmp -s "$SRC_CONFIG" "$DST_CONFIG"; then
        echo "🧩 Syncing updated config: $SRC_CONFIG -> $DST_CONFIG"
        cp "$SRC_CONFIG" "$DST_CONFIG" || {
            echo "⚠️ Warning: Failed to copy $SRC_CONFIG to deploy directory";
        }
    else
        echo "✅ Config already in sync: $DST_CONFIG"
    fi
else
    echo "ℹ️ Note: $SRC_CONFIG not found in project root; skipping pre-copy"
fi

# Change to netlify-deploy directory
cd netlify-deploy

echo "📦 Deploying static files AND functions to Netlify..."
echo "🔧 Static files: ."
echo "⚡ Functions: ./netlify/functions"

# Deploy with both static files and functions
netlify deploy --prod --dir . --functions ./netlify/functions

if [ $? -eq 0 ]; then
    echo "✅ Deployment successful!"
    echo "🔗 Site: https://peppy-pothos-093b81.netlify.app"
    echo "⚡ Functions: https://peppy-pothos-093b81.netlify.app/.netlify/functions/oura-proxy"
    echo ""
    echo "🎯 IMPORTANT: Always use this script instead of drag & drop!"
    echo "   Drag & drop will overwrite functions and break the watchface."

    # Non-fatal post-deploy verification
    echo "🔎 Verifying deployed config page..."
    DEPLOYED_URL="https://peppy-pothos-093b81.netlify.app/pebble-static-config.html"
    # Fetch full content for checks (avoid truncation false negatives)
    CONTENT=$(curl -fsSL "$DEPLOYED_URL" 2>/dev/null)
    if [ -n "$CONTENT" ]; then
        echo "🛰️ Fetched deployed page. Running checks..."
        SENTINELS=(
          "<optgroup label=\"Traditional\">"
          "<optgroup label=\"Full Month\">"
          "<optgroup label=\"Short Month\">"
          "<optgroup label=\"With Weekday\">"
          ">Sat, Aug 17<"
          ">Show Seconds<"
          ">Compact Time<"
        )
        MISSING=0
        for S in "${SENTINELS[@]}"; do
          echo "$CONTENT" | grep -q "$S" || { echo "⚠️ Missing sentinel: $S"; MISSING=1; }
        done
        if [ $MISSING -eq 0 ]; then
          echo "✅ Verification passed: expected elements found in deployed page."
        else
          echo "⚠️ Verification warning: Some expected elements not found. Investigate if changes took effect."
        fi
    else
        echo "⚠️ Verification warning: Could not fetch deployed config page."
    fi
else
    echo "❌ Deployment failed!"
    exit 1
fi
