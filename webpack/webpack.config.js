var webpack = require('webpack')

module.exports = {

  entry: {
    Nexus3D: './Nexus3D.js',
  },

  output: {
    path: __dirname + '/dist',
    filename: '[name].js',
    libraryTarget: 'var',
    // `library` determines the name of the global variable
    library: '[name]'
  },

  // don't run Babel on your library's dependencies
  module: {
  },

  optimization: {
    minimize: false
  }    
}
