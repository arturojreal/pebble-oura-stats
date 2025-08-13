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
else
    echo "❌ Deployment failed!"
    exit 1
fi
