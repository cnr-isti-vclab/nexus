var webpack = require('webpack')

module.exports = {

  entry: {
    Nexus3D: './src/Nexus3D.js',
  },

  output: {
    path: __dirname + '/build',
    filename: 'nexus3D.js',
    libraryTarget: 'umd',
    library: 'Nexus3D',
    globalObject: 'this'
  },

  externals: {
    three: {
      commonjs: 'three',
      commonjs2: 'three',
      amd: 'three',
      root: 'THREE'
    }
  },

  optimization: {
    minimize: false
  }    
}
