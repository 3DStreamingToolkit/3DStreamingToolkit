const path = require('path');
const CleanWebpackPlugin = require('clean-webpack-plugin');
const LodashModuleReplacementPlugin = require('lodash-webpack-plugin');


module.exports = {
  entry: './src/index.js',
  output: {
    filename: 'js-3dtoolkit.js',
    path: path.resolve(__dirname, 'dist'),
    library: 'ThreeDToolkit'
  },

  plugins: [
    new CleanWebpackPlugin(['dist']),
    new LodashModuleReplacementPlugin()
  ],
  module: {
    rules: [
      {
        test: /\.js$/,
        exclude: /(node_modules|bower_components)/,
        use: {
          loader: 'babel-loader',
          options: {
            plugins: ['lodash'],
            presets: ['env']
          }
        }
      }
    ]
  }
};
