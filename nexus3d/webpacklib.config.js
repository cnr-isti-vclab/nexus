var webpack = require('webpack')

module.exports = {

  entry: {
    Nexus3D: './Nexus3D.js',
  },

  output: {
    path: __dirname + '/dist',
    filename: '[name].js',
    libraryTarget: 'var',
    library: '[name]'
  },

  optimization: {
    minimize: false
  }    
}
