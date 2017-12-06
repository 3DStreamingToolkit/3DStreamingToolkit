const merge = require('webpack-merge');
const webpack = require('webpack');
const common = require('./webpack.common.js');

module.exports = merge(common, {
  devtool: 'inline-source-map',
  devServer: {
    contentBase: './example',
    hot: true
  },
  plugins: [
    new webpack.HotModuleReplacementPlugin()
  ],
});
